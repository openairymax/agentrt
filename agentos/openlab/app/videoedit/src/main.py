# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Video Editing Platform Main Application
======================================

This module provides the main FastAPI application for the OpenLab Video Editing
platform. It implements RESTful API endpoints for video processing operations
including import, export, trimming, merging, effects, and format conversion.

Architecture:
- FastAPI for high-performance async API
- FFmpeg for video processing
- Async file handling for uploads/downloads
- Task-based processing with progress tracking

Features:
- Video file upload and validation
- Video trimming operations
- Video merging/concatenation
- Format conversion
- Effects and filters application
- Audio extraction
- Thumbnail generation
- GIF creation
- Subtitle handling
"""

import asyncio
import json
import logging
import os
import shutil
import tempfile
import traceback
import uuid
from contextlib import asynccontextmanager
from dataclasses import dataclass
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List, Optional

import yaml

try:
    from fastapi import FastAPI, Request, HTTPException, UploadFile, File, BackgroundTasks, status
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.responses import FileResponse, JSONResponse, StreamingResponse
    from fastapi.security import HTTPBearer
    from pydantic import BaseModel, Field
    import uvicorn
    FASTAPI_AVAILABLE = True
except ImportError:
    FASTAPI_AVAILABLE = False

try:
    import aiofiles
    AIOFILES_AVAILABLE = True
except ImportError:
    AIOFILES_AVAILABLE = False


from edit_pipeline import (
    EditPipeline,
    PipelineConfig,
    VideoValidator,
    TaskStatus,
    FilterType,
    VideoMetadata
)


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


@dataclass
class TrimRequest:
    """Request model for video trimming."""
    start_time: float = Field(..., ge=0, description="Start time in seconds")
    end_time: float = Field(..., gt=0, description="End time in seconds")
    output_format: str = Field(default=".mp4", description="Output format")


@dataclass
class MergeRequest:
    """Request model for video merging."""
    output_format: str = Field(default=".mp4", description="Output format")


@dataclass
class FilterRequest:
    """Request model for applying filters."""
    filter_type: str = Field(
        ..., description="Filter type: brightness, contrast, saturation, blur, grayscale, sepia")
    filter_value: float = Field(
        default=50.0, ge=0, le=100, description="Filter intensity value")
    output_format: str = Field(default=".mp4", description="Output format")


@dataclass
class ConvertRequest:
    """Request model for format conversion."""
    output_format: str = Field(...,
                               description="Target format: mp4, avi, mov, mkv, webm, gif")
    video_codec: Optional[str] = Field(
        None, description="Video codec override")
    bitrate: Optional[str] = Field(None, description="Bitrate override")


@dataclass
class GifRequest:
    """Request model for GIF creation."""
    start_time: float = Field(..., ge=0, description="Start time in seconds")
    duration: float = Field(..., gt=0, le=30,
                            description="GIF duration in seconds")
    fps: int = Field(default=10, ge=1, le=30, description="Frames per second")
    width: int = Field(default=320, ge=100, le=800, description="GIF width")


@dataclass
class AudioExtractRequest:
    """Request model for audio extraction."""
    output_format: str = Field(
        default="mp3", description="Audio format: mp3, aac, wav")


@dataclass
class SubtitleRequest:
    """Request model for adding subtitles."""
    subtitle_path: str = Field(...,
                               description="Path to subtitle file (SRT format)")
    output_format: str = Field(default=".mp4", description="Output format")


class VideoEditApp:
    """Main video editing application class."""

    def __init__(self, config_path: str = "manager.yaml"):
        """Initialize the video editing application."""
        self.manager = PipelineConfig()
        self.manager.load(config_path)

        self.pipeline = EditPipeline(
            self.manager._config or self.manager._default_config())

        self.upload_dir = os.path.join(
            os.path.dirname(__file__), "..", "uploads")
        self.output_dir = self.manager.get("output_dir", "./output")
        os.makedirs(self.upload_dir, exist_ok=True)
        os.makedirs(self.output_dir, exist_ok=True)

        self.app = None
        self._setup_fastapi()

    def _setup_fastapi(self):
        """Setup FastAPI application with routes."""
        if not FASTAPI_AVAILABLE:
            logger.error("FastAPI not available")
            return

        @asynccontextmanager
        async def lifespan(app: FastAPI):
            """Application lifespan handler."""
            logger.info("Video editing application starting up")
            yield
            logger.info("Video editing application shutting down")

        self.app = FastAPI(
            title="OpenLab Video Editing Platform",
            description="A comprehensive video editing solution for the OpenLab marketplace",
            version="1.0.0",
            lifespan=lifespan
        )

        cors_origins = self.manager.get(
            "cors_origins", ["http://localhost:3000"])
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=cors_origins,
            allow_credentials=True,
            allow_methods=["*"],
            allow_headers=["*"],
        )

        self._setup_routes()

    def _setup_routes(self):
        """Setup API routes."""
        if self.app is None:
            return

        @self.app.get("/")
        async def root():
            """Root endpoint."""
            return {
                "service": "openlab Video Editing Platform",
                "version": "1.0.0",
                "status": "operational"
            }

        @self.app.get("/health")
        async def health_check():
            """Health check endpoint."""
            return {
                "status": "healthy",
                "timestamp": datetime.utcnow().isoformat(),
                "ffmpeg_available": self.pipeline.ffmpeg._check_ffmpeg()
            }

        @self.app.post("/api/upload")
        async def upload_video(file: UploadFile = File(...)):
            """Upload a video file for processing."""
            try:
                if not file.filename:
                    raise HTTPException(
                        status_code=400, detail="No file provided")

                ext = Path(file.filename).suffix.lower()
                supported = self.manager.get(
                    "supported_formats", {}).get("input", [])
                if ext not in supported:
                    raise HTTPException(
                        status_code=400,
                        detail=f"Unsupported file format: {ext}. Supported: {supported}"
                    )

                max_size = self.manager.get("max_file_size", 5368709120)

                file_id = str(uuid.uuid4())
                filename = f"{file_id}{ext}"
                filepath = os.path.join(self.upload_dir, filename)

                if AIOFILES_AVAILABLE:
                    async with aiofiles.open(filepath, 'wb') as f:
                        content = await file.read(1024 * 1024)
                        while content:
                            await f.write(content)
                            content = await file.read(1024 * 1024)
                else:
                    with open(filepath, 'wb') as f:
                        shutil.copyfileobj(file.file, f)

                file_size = os.path.getsize(filepath)
                if file_size > max_size:
                    os.remove(filepath)
                    raise HTTPException(
                        status_code=400, detail="File size exceeds maximum allowed")

                metadata = self.pipeline.ffmpeg.get_metadata(filepath)

                return {
                    "file_id": file_id,
                    "filename": file.filename,
                    "size": file_size,
                    "path": filepath,
                    "metadata": {
                        "duration": metadata.duration if metadata else None,
                        "width": metadata.width if metadata else None,
                        "height": metadata.height if metadata else None,
                        "fps": metadata.fps if metadata else None,
                        "codec": metadata.codec if metadata else None
                    } if metadata else None
                }

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Upload failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"Upload failed: {str(e)}")

        @self.app.post("/api/trim/{file_id}")
        async def trim_video(file_id: str, request: TrimRequest):
            """Trim a video file to specified time range."""
            try:
                filepath = self._find_uploaded_file(file_id)
                if not filepath:
                    raise HTTPException(
                        status_code=404, detail="File not found")

                metadata = self.pipeline.ffmpeg.get_metadata(filepath)
                if not metadata:
                    raise HTTPException(
                        status_code=400, detail="Could not read video metadata")

                if request.start_time >= metadata.duration:
                    raise HTTPException(
                        status_code=400, detail="Start time exceeds video duration")
                if request.end_time > metadata.duration:
                    raise HTTPException(
                        status_code=400, detail="End time exceeds video duration")
                if request.start_time >= request.end_time:
                    raise HTTPException(
                        status_code=400, detail="Start time must be less than end time")

                result = await self.pipeline.process_trim(
                    filepath, request.start_time, request.end_time, request.output_format
                )

                if result.status == TaskStatus.COMPLETED:
                    return {
                        "task_id": result.task_id,
                        "status": "completed",
                        "output_path": result.output_path,
                        "duration": result.duration,
                        "progress": result.progress
                    }
                else:
                    raise HTTPException(
                        status_code=500, detail=result.error or "Processing failed")

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Trim failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"Trim failed: {str(e)}")

        @self.app.post("/api/merge")
        async def merge_videos(file_ids: List[str], request: MergeRequest):
            """Merge multiple video files into one."""
            try:
                filepaths = []
                for file_id in file_ids:
                    filepath = self._find_uploaded_file(file_id)
                    if not filepath:
                        raise HTTPException(
                            status_code=404, detail=f"File not found: {file_id}")
                    filepaths.append(filepath)

                if len(filepaths) < 2:
                    raise HTTPException(
                        status_code=400, detail="At least 2 files required for merging")

                result = await self.pipeline.process_merge(filepaths, request.output_format)

                if result.status == TaskStatus.COMPLETED:
                    return {
                        "task_id": result.task_id,
                        "status": "completed",
                        "output_path": result.output_path,
                        "duration": result.duration,
                        "progress": result.progress
                    }
                else:
                    raise HTTPException(
                        status_code=500, detail=result.error or "Processing failed")

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Merge failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"Merge failed: {str(e)}")

        @self.app.post("/api/filter/{file_id}")
        async def apply_filter(file_id: str, request: FilterRequest):
            """Apply a filter effect to a video."""
            try:
                filepath = self._find_uploaded_file(file_id)
                if not filepath:
                    raise HTTPException(
                        status_code=404, detail="File not found")

                try:
                    FilterType[request.filter_type.upper()]
                except KeyError:
                    raise HTTPException(
                        status_code=400,
                        detail=f"Invalid filter type: {request.filter_type}"
                    )

                result = await self.pipeline.process_filter(
                    filepath, request.filter_type, request.filter_value, request.output_format
                )

                if result.status == TaskStatus.COMPLETED:
                    return {
                        "task_id": result.task_id,
                        "status": "completed",
                        "output_path": result.output_path,
                        "duration": result.duration,
                        "progress": result.progress
                    }
                else:
                    raise HTTPException(
                        status_code=500, detail=result.error or "Processing failed")

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Filter failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"Filter failed: {str(e)}")

        @self.app.post("/api/convert/{file_id}")
        async def convert_format(file_id: str, request: ConvertRequest):
            """Convert video to a different format."""
            try:
                filepath = self._find_uploaded_file(file_id)
                if not filepath:
                    raise HTTPException(
                        status_code=404, detail="File not found")

                result = await self.pipeline.process_format_conversion(
                    filepath, request.output_format, request.video_codec, request.bitrate
                )

                if result.status == TaskStatus.COMPLETED:
                    return {
                        "task_id": result.task_id,
                        "status": "completed",
                        "output_path": result.output_path,
                        "duration": result.duration,
                        "progress": result.progress
                    }
                else:
                    raise HTTPException(
                        status_code=500, detail=result.error or "Processing failed")

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Convert failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"Convert failed: {str(e)}")

        @self.app.post("/api/thumbnails/{file_id}")
        async def extract_thumbnails(
            file_id: str,
            count: Optional[int] = None,
            width: Optional[int] = None,
            height: Optional[int] = None
        ):
            """Extract thumbnails from a video."""
            try:
                filepath = self._find_uploaded_file(file_id)
                if not filepath:
                    raise HTTPException(
                        status_code=404, detail="File not found")

                result = await self.pipeline.process_thumbnail_extraction(
                    filepath, count, width, height
                )

                if result.status == TaskStatus.COMPLETED:
                    return {
                        "task_id": result.task_id,
                        "status": "completed",
                        "output_path": result.output_path,
                        "thumbnails": result.metadata.get("thumbnails", []) if result.metadata else [],
                        "duration": result.duration,
                        "progress": result.progress
                    }
                else:
                    raise HTTPException(
                        status_code=500, detail=result.error or "Processing failed")

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Thumbnail extraction failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"Thumbnail extraction failed: {str(e)}")

        @self.app.post("/api/gif/{file_id}")
        async def create_gif(file_id: str, request: GifRequest):
            """Create a GIF from a video clip."""
            try:
                filepath = self._find_uploaded_file(file_id)
                if not filepath:
                    raise HTTPException(
                        status_code=404, detail="File not found")

                result = await self.pipeline.process_gif_creation(
                    filepath, request.start_time, request.duration, request.fps, request.width
                )

                if result.status == TaskStatus.COMPLETED:
                    return {
                        "task_id": result.task_id,
                        "status": "completed",
                        "output_path": result.output_path,
                        "duration": result.duration,
                        "progress": result.progress
                    }
                else:
                    raise HTTPException(
                        status_code=500, detail=result.error or "Processing failed")

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"GIF creation failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"GIF creation failed: {str(e)}")

        @self.app.post("/api/audio/{file_id}")
        async def extract_audio(file_id: str, request: AudioExtractRequest):
            """Extract audio from a video."""
            try:
                filepath = self._find_uploaded_file(file_id)
                if not filepath:
                    raise HTTPException(
                        status_code=404, detail="File not found")

                result = await self.pipeline.process_audio_extraction(
                    filepath, request.output_format
                )

                if result.status == TaskStatus.COMPLETED:
                    return {
                        "task_id": result.task_id,
                        "status": "completed",
                        "output_path": result.output_path,
                        "duration": result.duration,
                        "progress": result.progress
                    }
                else:
                    raise HTTPException(
                        status_code=500, detail=result.error or "Processing failed")

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Audio extraction failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"Audio extraction failed: {str(e)}")

        @self.app.post("/api/subtitles/{file_id}")
        async def add_subtitles(file_id: str, request: SubtitleRequest):
            """Add subtitles to a video."""
            try:
                filepath = self._find_uploaded_file(file_id)
                if not filepath:
                    raise HTTPException(
                        status_code=404, detail="Video file not found")

                subtitle_path = request.subtitle_path
                if not os.path.exists(subtitle_path):
                    raise HTTPException(
                        status_code=404, detail="Subtitle file not found")

                result = await self.pipeline.process_subtitle_addition(
                    filepath, subtitle_path, request.output_format
                )

                if result.status == TaskStatus.COMPLETED:
                    return {
                        "task_id": result.task_id,
                        "status": "completed",
                        "output_path": result.output_path,
                        "duration": result.duration,
                        "progress": result.progress
                    }
                else:
                    raise HTTPException(
                        status_code=500, detail=result.error or "Processing failed")

            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Subtitle addition failed: {e}")
                raise HTTPException(
                    status_code=500, detail=f"Subtitle addition failed: {str(e)}")

        @self.app.get("/api/task/{task_id}")
        async def get_task_status(task_id: str):
            """Get the status of a processing task."""
            result = self.pipeline.get_task_status(task_id)
            if not result:
                raise HTTPException(status_code=404, detail="Task not found")

            return {
                "task_id": result.task_id,
                "status": result.status.value,
                "output_path": result.output_path,
                "progress": result.progress,
                "duration": result.duration,
                "error": result.error,
                "metadata": result.metadata
            }

        @self.app.post("/api/task/{task_id}/cancel")
        async def cancel_task(task_id: str):
            """Cancel a running task."""
            success = self.pipeline.cancel_task(task_id)
            if success:
                return {"message": "Task cancelled", "task_id": task_id}
            raise HTTPException(status_code=404, detail="Task not found")

        @self.app.get("/api/download/{filename}")
        async def download_file(filename: str):
            """Download a processed video file."""
            filepath = os.path.join(self.output_dir, filename)
            if not os.path.exists(filepath):
                raise HTTPException(status_code=404, detail="File not found")
            return FileResponse(
                filepath,
                media_type="application/octet-stream",
                filename=filename
            )

        @self.app.delete("/api/file/{file_id}")
        async def delete_uploaded_file(file_id: str):
            """Delete an uploaded file."""
            filepath = self._find_uploaded_file(file_id)
            if filepath and os.path.exists(filepath):
                os.remove(filepath)
                return {"message": "File deleted", "file_id": file_id}
            raise HTTPException(status_code=404, detail="File not found")

        @self.app.get("/api/formats")
        async def get_supported_formats():
            """Get list of supported video formats."""
            return {
                "input": self.manager.get("supported_formats", {}).get("input", []),
                "output": self.manager.get("supported_formats", {}).get("output", []),
                "filters": [f.name.lower() for f in FilterType],
                "codecs": {
                    "video": ["libx264", "libx265", "libvpx", "libvpx-vp9", "mpeg4"],
                    "audio": ["aac", "libmp3lame", "libopus", "libvorbis"]
                }
            }

        @self.app.post("/api/cleanup")
        async def cleanup_temp_files(hours: Optional[int] = None):
            """Clean up temporary files."""
            cleanup_hours = hours or self.manager.get(
                "tasks.auto_cleanup_hours", 24)
            count = self.pipeline.cleanup_temp_files(cleanup_hours)
            return {"message": f"Cleaned up {count} temporary files", "count": count}

    def _find_uploaded_file(self, file_id: str) -> Optional[str]:
        """Find an uploaded file by its ID."""
        if not os.path.exists(self.upload_dir):
            return None

        for filename in os.listdir(self.upload_dir):
            if filename.startswith(file_id):
                return os.path.join(self.upload_dir, filename)
        return None

    def run(self, host: Optional[str] = None, port: Optional[int] = None, debug: bool = False):
        """Run the FastAPI server."""
        if self.app is None:
            logger.error("FastAPI application not initialized")
            return

        host = host or self.manager.get("host", "0.0.0.0")
        port = port or self.manager.get("port", 8001)
        debug = debug or self.manager.get("debug", False)

        logger.info(f"Starting video editing server on {host}:{port}")
        uvicorn.run(self.app, host=host, port=port, debug=debug)


def main():
    """Main entry point for the video editing application."""
    import argparse

    parser = argparse.ArgumentParser(
        description="openlab Video Editing Platform")
    parser.add_argument("--manager", "-c", default="manager.yaml",
                        help="Path to configuration file")
    parser.add_argument("--host", "-h", default=None, help="Server host")
    parser.add_argument("--port", "-p", type=int,
                        default=None, help="Server port")
    parser.add_argument("--debug", "-d", action="store_true",
                        help="Enable debug mode")
    args = parser.parse_args()

    app = VideoEditApp(config_path=args.manager)
    app.run(host=args.host, port=args.port, debug=args.debug)


if __name__ == "__main__":
    main()
