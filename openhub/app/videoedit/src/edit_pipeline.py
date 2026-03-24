# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Video Editing Pipeline Module
=============================

This module provides the core video editing pipeline for the OpenHub Video Editing
platform. It handles video processing operations including trimming, merging,
effects application, format conversion, and audio processing.

Architecture:
- Pipeline-based processing with async support
- FFmpeg integration for video manipulation
- Support for multiple video formats and codecs
- Task queuing and progress tracking

Features:
- Video trimming and cutting
- Video merging and concatenation
- Format conversion
- Effects and filters
- Audio processing
- Thumbnail extraction
- Subtitle handling
"""

import asyncio
import json
import logging
import os
import subprocess
import tempfile
import time
import uuid
from dataclasses import dataclass, field
from datetime import datetime
from enum import Enum
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union

import yaml

try:
    import cv2
    import numpy as np
    from PIL import Image
    OPENCV_AVAILABLE = True
except ImportError:
    OPENCV_AVAILABLE = False

try:
    from moviepy.editor import VideoFileClip, AudioFileClip, CompositeVideoClip, concatenate_videoclips
    from moviepy.video.fx import all as vfx
    from moviepy.audio.fx import all as afx
    MOVIEPY_AVAILABLE = True
except ImportError:
    MOVIEPY_AVAILABLE = False


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


class VideoFormat(Enum):
    """Supported video formats."""
    MP4 = "mp4"
    AVI = "avi"
    MOV = "mov"
    MKV = "mkv"
    WEBM = "webm"
    GIF = "gif"
    FLV = "flv"
    WMV = "wmv"
    MPEG = "mpeg"


class VideoCodec(Enum):
    """Video codec options."""
    H264 = "libx264"
    H265 = "libx265"
    VP8 = "libvpx"
    VP9 = "libvpx-vp9"
    MPEG4 = "mpeg4"
    GIF_CODEC = "gif"


class AudioCodec(Enum):
    """Audio codec options."""
    AAC = "aac"
    MP3 = "libmp3lame"
    OPUS = "libopus"
    VORBIS = "libvorbis"
    WAV = "pcm_s16le"


class TransitionType(Enum):
    """Video transition types."""
    FADE = "fade"
    DISSOLVE = "dissolve"
    WIPE = "wipe"
    SLIDE = "slide"


class FilterType(Enum):
    """Video filter types."""
    BRIGHTNESS = "brightness"
    CONTRAST = "contrast"
    SATURATION = "saturation"
    BLUR = "blur"
    SHARPEN = "sharpen"
    GRAYSCALE = "grayscale"
    SEPIA = "sepia"
    INVERT = "invert"
    POSTERIZE = "posterize"


class TaskStatus(Enum):
    """Task execution status."""
    PENDING = "pending"
    RUNNING = "running"
    COMPLETED = "completed"
    FAILED = "failed"
    CANCELLED = "cancelled"


@dataclass
class VideoMetadata:
    """Video file metadata."""
    file_path: str
    duration: float
    width: int
    height: int
    fps: float
    codec: str
    audio_codec: Optional[str]
    audio_channels: int
    audio_sample_rate: int
    bitrate: int
    file_size: int
    format: str


@dataclass
class ClipInfo:
    """Information about a video clip."""
    source_path: str
    start_time: float
    end_time: float
    duration: float
    effects: List[Dict[str, Any]] = field(default_factory=list)
    volume: float = 1.0
    speed: float = 1.0


@dataclass
class TransitionInfo:
    """Information about a transition between clips."""
    type: TransitionType
    duration: float
    direction: str = "in"


@dataclass
class TaskResult:
    """Result of a video processing task."""
    task_id: str
    status: TaskStatus
    output_path: Optional[str]
    metadata: Optional[Dict[str, Any]]
    error: Optional[str]
    duration: float
    progress: float = 0.0


class VideoValidator:
    """Validates video files and parameters."""

    SUPPORTED_EXTENSIONS = ['.mp4', '.avi', '.mov', '.mkv', '.flv', '.wmv', '.webm', '.mpeg', '.mpg', '.3gp']

    @staticmethod
    def validate_file(file_path: str, max_size: Optional[int] = None) -> Tuple[bool, Optional[str]]:
        """Validate if a video file is valid and acceptable."""
        if not os.path.exists(file_path):
            return False, f"File not found: {file_path}"

        if max_size and os.path.getsize(file_path) > max_size:
            return False, f"File size exceeds maximum allowed size"

        ext = Path(file_path).suffix.lower()
        if ext not in VideoValidator.SUPPORTED_EXTENSIONS:
            return False, f"Unsupported file format: {ext}"

        return True, None

    @staticmethod
    def validate_time_range(start_time: float, end_time: float, duration: float) -> Tuple[bool, Optional[str]]:
        """Validate if time range is within video duration."""
        if start_time < 0:
            return False, "Start time cannot be negative"
        if end_time <= start_time:
            return False, "End time must be greater than start time"
        if end_time > duration:
            return False, f"End time exceeds video duration ({duration}s)"
        return True, None

    @staticmethod
    def validate_resolution(width: int, height: int) -> Tuple[bool, Optional[str]]:
        """Validate video resolution."""
        if width <= 0 or height <= 0:
            return False, "Resolution dimensions must be positive"
        if width % 2 != 0 or height % 2 != 0:
            return False, "Resolution dimensions must be even"
        if width > 7680 or height > 4320:
            return False, "Resolution exceeds maximum (8K)"
        return True, None


class FFmpegWrapper:
    """Wrapper for FFmpeg operations."""

    def __init__(self, ffmpeg_path: str = "ffmpeg", timeout: int = 3600):
        self.ffmpeg_path = ffmpeg_path
        self.timeout = timeout
        self._check_ffmpeg()

    def _check_ffmpeg(self) -> bool:
        """Check if FFmpeg is available."""
        try:
            result = subprocess.run(
                [self.ffmpeg_path, "-version"],
                capture_output=True,
                text=True,
                timeout=5
            )
            if result.returncode == 0:
                logger.info(f"FFmpeg available: {result.stdout.split()[2]}")
                return True
        except Exception as e:
            logger.warning(f"FFmpeg not available: {e}")
        return False

    def get_metadata(self, file_path: str) -> Optional[VideoMetadata]:
        """Extract metadata from a video file using ffprobe."""
        try:
            cmd = [
                "ffprobe",
                "-v", "quiet",
                "-print_format", "json",
                "-show_format",
                "-show_streams",
                file_path
            ]
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            if result.returncode != 0:
                return None

            data = json.loads(result.stdout)
            video_stream = next((s for s in data.get("streams", []) if s.get("codec_type") == "video"), None)
            audio_stream = next((s for s in data.get("streams", []) if s.get("codec_type") == "audio"), None)
            format_info = data.get("format", {})

            if not video_stream:
                return None

            return VideoMetadata(
                file_path=file_path,
                duration=float(format_info.get("duration", 0)),
                width=int(video_stream.get("width", 0)),
                height=int(video_stream.get("height", 0)),
                fps=eval(video_stream.get("r_frame_rate", "0/1")) if video_stream.get("r_frame_rate") else 0,
                codec=video_stream.get("codec_name", "unknown"),
                audio_codec=audio_stream.get("codec_name") if audio_stream else None,
                audio_channels=int(audio_stream.get("channels", 0)) if audio_stream else 0,
                audio_sample_rate=int(audio_stream.get("sample_rate", 0)) if audio_stream else 0,
                bitrate=int(format_info.get("bit_rate", 0)),
                file_size=int(format_info.get("size", 0)),
                format=format_info.get("format_name", "").split(",")[0]
            )
        except Exception as e:
            logger.error(f"Failed to get metadata: {e}")
            return None

    async def trim_video(
        self,
        input_path: str,
        output_path: str,
        start_time: float,
        end_time: float,
        codec: str = "libx264",
        preset: str = "medium",
        crf: int = 23
    ) -> Tuple[bool, Optional[str]]:
        """Trim a video to a specific time range."""
        duration = end_time - start_time
        cmd = [
            self.ffmpeg_path,
            "-y",
            "-ss", str(start_time),
            "-i", input_path,
            "-t", str(duration),
            "-c:v", codec,
            "-preset", preset,
            "-crf", str(crf),
            "-c:a", "aac",
            "-strict", "experimental",
            output_path
        ]

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=self.timeout)

            if process.returncode == 0:
                return True, None
            return False, stderr.decode()
        except asyncio.TimeoutError:
            process.kill()
            return False, "FFmpeg operation timed out"
        except Exception as e:
            return False, str(e)

    async def merge_videos(
        self,
        input_paths: List[str],
        output_path: str,
        codec: str = "libx264",
        preset: str = "medium",
        crf: int = 23
    ) -> Tuple[bool, Optional[str]]:
        """Merge multiple videos into one."""
        if len(input_paths) < 2:
            return False, "At least 2 input files required for merging"

        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as f:
            for path in input_paths:
                f.write(f"file '{path}'\n")
            list_file = f.name

        try:
            cmd = [
                self.ffmpeg_path,
                "-y",
                "-f", "concat",
                "-safe", "0",
                "-i", list_file,
                "-c:v", codec,
                "-preset", preset,
                "-crf", str(crf),
                "-c:a", "aac",
                "-strict", "experimental",
                output_path
            ]

            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=self.timeout)

            if process.returncode == 0:
                return True, None
            return False, stderr.decode()
        except asyncio.TimeoutError:
            process.kill()
            return False, "FFmpeg operation timed out"
        except Exception as e:
            return False, str(e)
        finally:
            if os.path.exists(list_file):
                os.unlink(list_file)

    async def apply_filter(
        self,
        input_path: str,
        output_path: str,
        filter_type: FilterType,
        filter_value: float,
        codec: str = "libx264",
        preset: str = "medium"
    ) -> Tuple[bool, Optional[str]]:
        """Apply a video filter."""
        filter_str = self._get_filter_string(filter_type, filter_value)

        cmd = [
            self.ffmpeg_path,
            "-y",
            "-i", input_path,
            "-vf", filter_str,
            "-c:v", codec,
            "-preset", preset,
            "-c:a", "aac",
            "-strict", "experimental",
            output_path
        ]

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=self.timeout)

            if process.returncode == 0:
                return True, None
            return False, stderr.decode()
        except asyncio.TimeoutError:
            process.kill()
            return False, "FFmpeg operation timed out"
        except Exception as e:
            return False, str(e)

    def _get_filter_string(self, filter_type: FilterType, value: float) -> str:
        """Convert filter type and value to FFmpeg filter string."""
        filter_map = {
            FilterType.BRIGHTNESS: f"eq=brightness={value/100}",
            FilterType.CONTRAST: f"eq=contrast={value/100}",
            FilterType.SATURATION: f"eq=saturation={value/100}",
            FilterType.BLUR: f"boxblur={int(value)}",
            FilterType.GRAYSCALE: "hue=s=0",
            FilterType.SEPIA: "colorchannelmixer=.393:.769:.189:0:.349:.686:.168:0:.272:.534:.131",
        }
        return filter_map.get(filter_type, "")

    async def extract_thumbnails(
        self,
        input_path: str,
        output_dir: str,
        count: int = 3,
        width: int = 320,
        height: int = 180
    ) -> Tuple[bool, List[str], Optional[str]]:
        """Extract thumbnails from a video."""
        os.makedirs(output_dir, exist_ok=True)
        duration = self.get_duration(input_path)
        if not duration:
            return False, [], "Could not determine video duration"

        interval = duration / (count + 1)
        output_paths = []
        success = True
        error_msg = None

        for i in range(count):
            timestamp = interval * (i + 1)
            output_path = os.path.join(output_dir, f"thumbnail_{i:04d}.jpg")

            cmd = [
                self.ffmpeg_path,
                "-y",
                "-ss", str(timestamp),
                "-i", input_path,
                "-vframes", "1",
                "-vf", f"scale={width}:{height}:force_original_aspect_ratio=decrease,pad={width}:{height}:(ow-iw)/2:(oh-ih)/2",
                "-q:v", "2",
                output_path
            ]

            try:
                process = await asyncio.create_subprocess_exec(
                    *cmd,
                    stdout=asyncio.subprocess.PIPE,
                    stderr=asyncio.subprocess.PIPE
                )
                stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=30)

                if process.returncode == 0 and os.path.exists(output_path):
                    output_paths.append(output_path)
                else:
                    success = False
                    error_msg = stderr.decode()
            except Exception as e:
                success = False
                error_msg = str(e)

        return success, output_paths, error_msg if not success else None

    async def convert_format(
        self,
        input_path: str,
        output_path: str,
        video_codec: str = "libx264",
        audio_codec: str = "aac",
        bitrate: str = "2M",
        preset: str = "medium",
        crf: int = 23
    ) -> Tuple[bool, Optional[str]]:
        """Convert video to a different format."""
        cmd = [
            self.ffmpeg_path,
            "-y",
            "-i", input_path,
            "-c:v", video_codec,
            "-preset", preset,
            "-crf", str(crf),
            "-b:v", bitrate,
            "-c:a", audio_codec,
            "-strict", "experimental",
            output_path
        ]

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=self.timeout)

            if process.returncode == 0:
                return True, None
            return False, stderr.decode()
        except asyncio.TimeoutError:
            process.kill()
            return False, "FFmpeg operation timed out"
        except Exception as e:
            return False, str(e)

    async def extract_audio(
        self,
        input_path: str,
        output_path: str,
        audio_codec: str = "aac",
        bitrate: str = "128k"
    ) -> Tuple[bool, Optional[str]]:
        """Extract audio from a video."""
        cmd = [
            self.ffmpeg_path,
            "-y",
            "-i", input_path,
            "-vn",
            "-c:a", audio_codec,
            "-b:a", bitrate,
            output_path
        ]

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=self.timeout)

            if process.returncode == 0:
                return True, None
            return False, stderr.decode()
        except asyncio.TimeoutError:
            process.kill()
            return False, "FFmpeg operation timed out"
        except Exception as e:
            return False, str(e)

    async def add_subtitles(
        self,
        input_path: str,
        output_path: str,
        subtitle_path: str,
        codec: str = "libx264"
    ) -> Tuple[bool, Optional[str]]:
        """Add subtitles to a video."""
        cmd = [
            self.ffmpeg_path,
            "-y",
            "-i", input_path,
            "-vf", f"subtitles='{subtitle_path}'",
            "-c:v", codec,
            "-c:a", "aac",
            "-strict", "experimental",
            output_path
        ]

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=self.timeout)

            if process.returncode == 0:
                return True, None
            return False, stderr.decode()
        except asyncio.TimeoutError:
            process.kill()
            return False, "FFmpeg operation timed out"
        except Exception as e:
            return False, str(e)

    def get_duration(self, file_path: str) -> Optional[float]:
        """Get video duration without loading the entire file."""
        cmd = [
            "ffprobe",
            "-v", "error",
            "-show_entries", "format=duration",
            "-of", "default=noprint_wrappers=1:nokey=1",
            file_path
        ]
        try:
            result = subprocess.run(cmd, capture_output=True, text=True, timeout=30)
            if result.returncode == 0:
                return float(result.stdout.strip())
        except Exception as e:
            logger.error(f"Failed to get duration: {e}")
        return None

    async def create_gif(
        self,
        input_path: str,
        output_path: str,
        start_time: float,
        duration: float,
        fps: int = 10,
        width: int = 320
    ) -> Tuple[bool, Optional[str]]:
        """Create a GIF from a video clip."""
        cmd = [
            self.ffmpeg_path,
            "-y",
            "-ss", str(start_time),
            "-t", str(duration),
            "-i", input_path,
            "-vf", f"fps={fps},scale={width}:-1:flags=lanczos,split[s0][s1];[s0]palettegen[p];[s1][p]paletteuse",
            "-loop", "0",
            output_path
        ]

        try:
            process = await asyncio.create_subprocess_exec(
                *cmd,
                stdout=asyncio.subprocess.PIPE,
                stderr=asyncio.subprocess.PIPE
            )
            stdout, stderr = await asyncio.wait_for(process.communicate(), timeout=self.timeout)

            if process.returncode == 0:
                return True, None
            return False, stderr.decode()
        except asyncio.TimeoutError:
            process.kill()
            return False, "FFmpeg operation timed out"
        except Exception as e:
            return False, str(e)


class EditPipeline:
    """Main video editing pipeline orchestrator."""

    def __init__(self, config: Dict[str, Any]):
        """Initialize the edit pipeline with configuration."""
        self.config = config
        self.ffmpeg = FFmpegWrapper(
            ffmpeg_path=config.get("ffmpeg", {}).get("path", "ffmpeg"),
            timeout=config.get("ffmpeg", {}).get("timeout", 3600)
        )
        self.output_dir = config.get("output_dir", "./output")
        self.temp_dir = config.get("temp_dir", "./temp")
        self.tasks: Dict[str, TaskResult] = {}
        os.makedirs(self.output_dir, exist_ok=True)
        os.makedirs(self.temp_dir, exist_ok=True)

    def _generate_task_id(self) -> str:
        """Generate a unique task ID."""
        return f"task_{uuid.uuid4().hex[:12]}"

    def _get_output_path(self, task_id: str, extension: str) -> str:
        """Get output file path for a task."""
        return os.path.join(self.output_dir, f"{task_id}{extension}")

    async def process_trim(
        self,
        input_path: str,
        start_time: float,
        end_time: float,
        output_format: str = ".mp4"
    ) -> TaskResult:
        """Process a video trim operation."""
        task_id = self._generate_task_id()
        output_path = self._get_output_path(task_id, output_format)
        start = time.time()

        self.tasks[task_id] = TaskResult(
            task_id=task_id,
            status=TaskStatus.RUNNING,
            output_path=None,
            metadata=None,
            error=None,
            duration=0.0,
            progress=0.0
        )

        try:
            metadata = self.ffmpeg.get_metadata(input_path)
            if not metadata:
                return self._fail_task(task_id, "Could not read input video metadata", start)

            valid, error = VideoValidator.validate_time_range(start_time, end_time, metadata.duration)
            if not valid:
                return self._fail_task(task_id, error, start)

            codec = self.config.get("video", {}).get("default_codec", "libx264")
            preset = self.config.get("ffmpeg", {}).get("preset", "medium")
            crf = self.config.get("ffmpeg", {}).get("crf", 23)

            self.tasks[task_id].progress = 0.5
            success, error = await self.ffmpeg.trim_video(
                input_path, output_path, start_time, end_time, codec, preset, crf
            )

            if success:
                return self._complete_task(task_id, output_path, start)
            return self._fail_task(task_id, error, start)

        except Exception as e:
            return self._fail_task(task_id, str(e), start)

    async def process_merge(
        self,
        input_paths: List[str],
        output_format: str = ".mp4"
    ) -> TaskResult:
        """Process a video merge operation."""
        task_id = self._generate_task_id()
        output_path = self._get_output_path(task_id, output_format)
        start = time.time()

        self.tasks[task_id] = TaskResult(
            task_id=task_id,
            status=TaskStatus.RUNNING,
            output_path=None,
            metadata=None,
            error=None,
            duration=0.0,
            progress=0.0
        )

        try:
            for path in input_paths:
                valid, error = VideoValidator.validate_file(path)
                if not valid:
                    return self._fail_task(task_id, f"Invalid input file: {error}", start)

            codec = self.config.get("video", {}).get("default_codec", "libx264")
            preset = self.config.get("ffmpeg", {}).get("preset", "medium")
            crf = self.config.get("ffmpeg", {}).get("crf", 23)

            self.tasks[task_id].progress = 0.5
            success, error = await self.ffmpeg.merge_videos(
                input_paths, output_path, codec, preset, crf
            )

            if success:
                return self._complete_task(task_id, output_path, start)
            return self._fail_task(task_id, error, start)

        except Exception as e:
            return self._fail_task(task_id, str(e), start)

    async def process_filter(
        self,
        input_path: str,
        filter_type: str,
        filter_value: float,
        output_format: str = ".mp4"
    ) -> TaskResult:
        """Process a video filter operation."""
        task_id = self._generate_task_id()
        output_path = self._get_output_path(task_id, output_format)
        start = time.time()

        self.tasks[task_id] = TaskResult(
            task_id=task_id,
            status=TaskStatus.RUNNING,
            output_path=None,
            metadata=None,
            error=None,
            duration=0.0,
            progress=0.0
        )

        try:
            try:
                ft = FilterType[filter_type.upper()]
            except KeyError:
                return self._fail_task(task_id, f"Unknown filter type: {filter_type}", start)

            codec = self.config.get("video", {}).get("default_codec", "libx264")
            preset = self.config.get("ffmpeg", {}).get("preset", "medium")

            self.tasks[task_id].progress = 0.5
            success, error = await self.ffmpeg.apply_filter(
                input_path, output_path, ft, filter_value, codec, preset
            )

            if success:
                return self._complete_task(task_id, output_path, start)
            return self._fail_task(task_id, error, start)

        except Exception as e:
            return self._fail_task(task_id, str(e), start)

    async def process_format_conversion(
        self,
        input_path: str,
        output_format: str,
        video_codec: Optional[str] = None,
        bitrate: Optional[str] = None
    ) -> TaskResult:
        """Process a format conversion operation."""
        task_id = self._generate_task_id()
        output_path = self._get_output_path(task_id, f".{output_format}")
        start = time.time()

        self.tasks[task_id] = TaskResult(
            task_id=task_id,
            status=TaskStatus.RUNNING,
            output_path=None,
            metadata=None,
            error=None,
            duration=0.0,
            progress=0.0
        )

        try:
            v_codec = video_codec or self.config.get("video", {}).get("default_codec", "libx264")
            a_codec = self.config.get("audio", {}).get("default_codec", "aac")
            br = bitrate or self.config.get("video", {}).get("default_bitrate", "2M")
            preset = self.config.get("ffmpeg", {}).get("preset", "medium")
            crf = self.config.get("ffmpeg", {}).get("crf", 23)

            self.tasks[task_id].progress = 0.5
            success, error = await self.ffmpeg.convert_format(
                input_path, output_path, v_codec, a_codec, br, preset, crf
            )

            if success:
                return self._complete_task(task_id, output_path, start)
            return self._fail_task(task_id, error, start)

        except Exception as e:
            return self._fail_task(task_id, str(e), start)

    async def process_thumbnail_extraction(
        self,
        input_path: str,
        count: Optional[int] = None,
        width: Optional[int] = None,
        height: Optional[int] = None
    ) -> TaskResult:
        """Process thumbnail extraction."""
        task_id = self._generate_task_id()
        output_dir = os.path.join(self.temp_dir, task_id)
        os.makedirs(output_dir, exist_ok=True)
        start = time.time()

        self.tasks[task_id] = TaskResult(
            task_id=task_id,
            status=TaskStatus.RUNNING,
            output_path=None,
            metadata=None,
            error=None,
            duration=0.0,
            progress=0.0
        )

        try:
            thumb_count = count or self.config.get("thumbnails", {}).get("default_count", 3)
            thumb_width = width or self.config.get("thumbnails", {}).get("default_width", 320)
            thumb_height = height or self.config.get("thumbnails", {}).get("default_height", 180)

            self.tasks[task_id].progress = 0.5
            success, paths, error = await self.ffmpeg.extract_thumbnails(
                input_path, output_dir, thumb_count, thumb_width, thumb_height
            )

            if success:
                metadata = {
                    "thumbnails": paths,
                    "count": len(paths),
                    "width": thumb_width,
                    "height": thumb_height
                }
                return self._complete_task(task_id, output_dir, start, metadata)
            return self._fail_task(task_id, error, start)

        except Exception as e:
            return self._fail_task(task_id, str(e), start)

    async def process_gif_creation(
        self,
        input_path: str,
        start_time: float,
        duration: float,
        fps: int = 10,
        width: int = 320
    ) -> TaskResult:
        """Process GIF creation from video clip."""
        task_id = self._generate_task_id()
        output_path = self._get_output_path(task_id, ".gif")
        start = time.time()

        self.tasks[task_id] = TaskResult(
            task_id=task_id,
            status=TaskStatus.RUNNING,
            output_path=None,
            metadata=None,
            error=None,
            duration=0.0,
            progress=0.0
        )

        try:
            metadata = self.ffmpeg.get_metadata(input_path)
            if not metadata:
                return self._fail_task(task_id, "Could not read input video metadata", start)

            valid, error = VideoValidator.validate_time_range(start_time, start_time + duration, metadata.duration)
            if not valid:
                return self._fail_task(task_id, error, start)

            self.tasks[task_id].progress = 0.5
            success, error = await self.ffmpeg.create_gif(
                input_path, output_path, start_time, duration, fps, width
            )

            if success:
                return self._complete_task(task_id, output_path, start)
            return self._fail_task(task_id, error, start)

        except Exception as e:
            return self._fail_task(task_id, str(e), start)

    async def process_audio_extraction(
        self,
        input_path: str,
        output_format: str = "mp3"
    ) -> TaskResult:
        """Process audio extraction from video."""
        task_id = self._generate_task_id()
        output_path = self._get_output_path(task_id, f".{output_format}")
        start = time.time()

        self.tasks[task_id] = TaskResult(
            task_id=task_id,
            status=TaskStatus.RUNNING,
            output_path=None,
            metadata=None,
            error=None,
            duration=0.0,
            progress=0.0
        )

        try:
            audio_codec = self.config.get("audio", {}).get("default_codec", "aac")
            bitrate = self.config.get("audio", {}).get("default_bitrate", "128k")

            self.tasks[task_id].progress = 0.5
            success, error = await self.ffmpeg.extract_audio(
                input_path, output_path, audio_codec, bitrate
            )

            if success:
                return self._complete_task(task_id, output_path, start)
            return self._fail_task(task_id, error, start)

        except Exception as e:
            return self._fail_task(task_id, str(e), start)

    async def process_subtitle_addition(
        self,
        input_path: str,
        subtitle_path: str,
        output_format: str = ".mp4"
    ) -> TaskResult:
        """Process adding subtitles to video."""
        task_id = self._generate_task_id()
        output_path = self._get_output_path(task_id, output_format)
        start = time.time()

        self.tasks[task_id] = TaskResult(
            task_id=task_id,
            status=TaskStatus.RUNNING,
            output_path=None,
            metadata=None,
            error=None,
            duration=0.0,
            progress=0.0
        )

        try:
            codec = self.config.get("video", {}).get("default_codec", "libx264")

            self.tasks[task_id].progress = 0.5
            success, error = await self.ffmpeg.add_subtitles(
                input_path, output_path, subtitle_path, codec
            )

            if success:
                return self._complete_task(task_id, output_path, start)
            return self._fail_task(task_id, error, start)

        except Exception as e:
            return self._fail_task(task_id, str(e), start)

    def get_task_status(self, task_id: str) -> Optional[TaskResult]:
        """Get the status of a processing task."""
        return self.tasks.get(task_id)

    def cancel_task(self, task_id: str) -> bool:
        """Cancel a running task."""
        if task_id in self.tasks:
            self.tasks[task_id].status = TaskStatus.CANCELLED
            return True
        return False

    def _complete_task(
        self,
        task_id: str,
        output_path: str,
        start_time: float,
        metadata: Optional[Dict[str, Any]] = None
    ) -> TaskResult:
        """Mark a task as completed."""
        self.tasks[task_id].status = TaskStatus.COMPLETED
        self.tasks[task_id].output_path = output_path
        self.tasks[task_id].metadata = metadata or {}
        self.tasks[task_id].duration = time.time() - start_time
        self.tasks[task_id].progress = 1.0
        return self.tasks[task_id]

    def _fail_task(self, task_id: str, error: str, start_time: float) -> TaskResult:
        """Mark a task as failed."""
        self.tasks[task_id].status = TaskStatus.FAILED
        self.tasks[task_id].error = error
        self.tasks[task_id].duration = time.time() - start_time
        return self.tasks[task_id]

    def cleanup_temp_files(self, hours: int = 24) -> int:
        """Clean up temporary files older than specified hours."""
        count = 0
        cutoff = time.time() - (hours * 3600)

        if os.path.exists(self.temp_dir):
            for filename in os.listdir(self.temp_dir):
                filepath = os.path.join(self.temp_dir, filename)
                if os.path.isfile(filepath) and os.path.getmtime(filepath) < cutoff:
                    try:
                        os.remove(filepath)
                        count += 1
                    except Exception as e:
                        logger.warning(f"Failed to remove temp file {filepath}: {e}")

        return count


class PipelineConfig:
    """Configuration manager for the video editing pipeline."""

    _instance = None
    _config = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def load(self, config_path: str = "config.yaml") -> Dict[str, Any]:
        """Load configuration from YAML file."""
        if self._config is None:
            try:
                with open(config_path, 'r') as f:
                    self._config = yaml.safe_load(f)
            except FileNotFoundError:
                logger.warning(f"Config file {config_path} not found, using defaults")
                self._config = self._default_config()
        return self._config

    def _default_config(self) -> Dict[str, Any]:
        """Return default configuration."""
        return {
            "output_dir": "./output",
            "temp_dir": "./temp",
            "max_file_size": 5368709120,
            "supported_formats": {
                "input": [".mp4", ".avi", ".mov", ".mkv", ".flv", ".wmv", ".webm"],
                "output": [".mp4", ".avi", ".mov", ".mkv", ".webm", ".gif"]
            },
            "ffmpeg": {
                "path": "ffmpeg",
                "timeout": 3600,
                "threads": 4,
                "preset": "medium",
                "crf": 23
            },
            "video": {
                "default_codec": "libx264",
                "default_audio_codec": "aac",
                "default_bitrate": "2M",
                "default_resolution": "1920x1080",
                "default_fps": 30
            },
            "audio": {
                "default_codec": "aac",
                "default_bitrate": "128k",
                "default_sample_rate": 44100
            },
            "thumbnails": {
                "default_count": 3,
                "default_width": 320,
                "default_height": 180
            },
            "tasks": {
                "max_concurrent": 2,
                "auto_cleanup_hours": 24
            }
        }

    def get(self, key: str, default: Any = None) -> Any:
        """Get configuration value by key."""
        if self._config is None:
            self.load()
        keys = key.split(".")
        value = self._config
        for k in keys:
            if isinstance(value, dict):
                value = value.get(k)
            else:
                return default
        return value if value is not None else default
