# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
OpenHub Docker Image
=================

Multi-stage Docker build for OpenHub AgentOS.
"""

FROM python:3.11-slim as base

ENV PYTHONUNBUFFERED=1 \
    PYTHONDONTWRITEBYTECODE=1 \
    PIP_NO_CACHE_DIR=1 \
    PIP_DISABLE_PIP_VERSION_CHECK=1

WORKDIR /app

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    curl \
    git \
    && rm -rf /var/lib/apt/lists/*

FROM base as builder

RUN python -m venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

COPY pyproject.toml ./
RUN pip install --upgrade pip && \
    pip install --no-cache-dir .

FROM base as production

COPY --from=builder /opt/venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH" \
    PYTHONUNBUFFERED=1

RUN mkdir -p /app/uploads /app/output /app/temp
ENV UPLOAD_DIR=/app/uploads \
    OUTPUT_DIR=/app/output \
    TEMP_DIR=/app/temp

COPY --from=builder /app /app

EXPOSE 8000 8001

HEALTHCHECK --interval=30s --timeout=10s --start-period=5s --retries=3 \
    CMD python -c "import urllib.request; urllib.request.urlopen('http://localhost:8000/health')"

CMD ["python", "-m", "uvicorn", "openhub.main:app", "--host", "0.0.0.0", "--port", "8000"]

FROM base as development

COPY --from=builder /opt/venv /opt/venv
ENV PATH="/opt/venv/bin:$PATH"

COPY . .

CMD ["python", "-m", "uvicorn", "openhub.main:app", "--host", "0.0.0.0", "--port", "8000", "--reload"]
