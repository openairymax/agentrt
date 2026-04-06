# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
Unit Tests for VideoEdit Application
=================================
"""

import pytest
import sys
import os

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))


class TestVideoEditPipeline:
    """Tests for video editing pipeline."""

    def test_pipeline_initialization(self):
        """Test pipeline can be initialized."""
        from App.videoedit.src.edit_pipeline import EditPipeline, PipelineConfig

        manager = PipelineConfig()._default_config()
        pipeline = EditPipeline(manager)

        assert pipeline is not None
        assert pipeline.output_dir == manager["output_dir"]
        assert pipeline.temp_dir == manager["temp_dir"]

    def test_generate_task_id(self):
        """Test task ID generation."""
        from App.videoedit.src.edit_pipeline import EditPipeline, PipelineConfig

        manager = PipelineConfig()._default_config()
        pipeline = EditPipeline(manager)

        task_id1 = pipeline._generate_task_id()
        task_id2 = pipeline._generate_task_id()

        assert task_id1.startswith("task_")
        assert task_id1 != task_id2


class TestVideoValidator:
    """Tests for video validation."""

    def test_validate_time_range_valid(self):
        """Test validating a valid time range."""
        from App.videoedit.src.edit_pipeline import VideoValidator

        valid, error = VideoValidator.validate_time_range(0.0, 10.0, 30.0)

        assert valid is True
        assert error is None

    def test_validate_time_range_invalid_start(self):
        """Test validating invalid start time."""
        from App.videoedit.src.edit_pipeline import VideoValidator

        valid, error = VideoValidator.validate_time_range(-1.0, 10.0, 30.0)

        assert valid is False
        assert "negative" in error.lower()

    def test_validate_time_range_end_before_start(self):
        """Test validating when end time is before start time."""
        from App.videoedit.src.edit_pipeline import VideoValidator

        valid, error = VideoValidator.validate_time_range(20.0, 10.0, 30.0)

        assert valid is False
        assert "greater" in error.lower()

    def test_validate_time_range_exceeds_duration(self):
        """Test validating when end time exceeds video duration."""
        from App.videoedit.src.edit_pipeline import VideoValidator

        valid, error = VideoValidator.validate_time_range(0.0, 40.0, 30.0)

        assert valid is False
        assert "exceeds" in error.lower()

    def test_validate_resolution_valid(self):
        """Test validating a valid resolution."""
        from App.videoedit.src.edit_pipeline import VideoValidator

        valid, error = VideoValidator.validate_resolution(1920, 1080)

        assert valid is True
        assert error is None

    def test_validate_resolution_odd_dimensions(self):
        """Test validating resolution with odd dimensions."""
        from App.videoedit.src.edit_pipeline import VideoValidator

        valid, error = VideoValidator.validate_resolution(1921, 1080)

        assert valid is False
        assert "even" in error.lower()

    def test_validate_resolution_too_large(self):
        """Test validating resolution that is too large."""
        from App.videoedit.src.edit_pipeline import VideoValidator

        valid, error = VideoValidator.validate_resolution(10000, 10000)

        assert valid is False
        assert "exceeds" in error.lower()


class TestVideoMetadata:
    """Tests for video metadata handling."""

    def test_video_metadata_creation(self):
        """Test creating video metadata."""
        from App.videoedit.src.edit_pipeline import VideoMetadata

        metadata = VideoMetadata(
            file_path="/path/to/video.mp4",
            duration=120.0,
            width=1920,
            height=1080,
            fps=30.0,
            codec="h264",
            audio_codec="aac",
            audio_channels=2,
            audio_sample_rate=44100,
            bitrate=5000000,
            file_size=75000000,
            format="mp4"
        )

        assert metadata.file_path == "/path/to/video.mp4"
        assert metadata.duration == 120.0
        assert metadata.width == 1920
        assert metadata.height == 1080


class TestClipInfo:
    """Tests for clip information."""

    def test_clip_info_creation(self):
        """Test creating clip info."""
        from App.videoedit.src.edit_pipeline import ClipInfo

        clip = ClipInfo(
            source_path="/path/to/video.mp4",
            start_time=0.0,
            end_time=30.0,
            duration=30.0
        )

        assert clip.source_path == "/path/to/video.mp4"
        assert clip.start_time == 0.0
        assert clip.end_time == 30.0
        assert clip.duration == 30.0
        assert clip.volume == 1.0
        assert clip.speed == 1.0


class TestTaskResult:
    """Tests for task result."""

    def test_task_result_creation(self):
        """Test creating task result."""
        from App.videoedit.src.edit_pipeline import TaskResult, TaskStatus

        result = TaskResult(
            task_id="task-001",
            status=TaskStatus.COMPLETED,
            output_path="/path/to/output.mp4",
            metadata={"duration": 30.0},
            error=None,
            duration=5.0,
            progress=1.0
        )

        assert result.task_id == "task-001"
        assert result.status == TaskStatus.COMPLETED
        assert result.output_path == "/path/to/output.mp4"
        assert result.progress == 1.0


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
