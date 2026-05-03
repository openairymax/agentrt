# Docker Swarm Secrets

This directory stores Docker Swarm secret files for production deployment.

## Usage

```bash
# Create a secret from a file
docker secret create agentos_jwt_secret ./jwt_secret.txt
docker secret create agentos_postgres_password ./postgres_password.txt

# List secrets
docker secret ls

# Use in docker-compose (Docker Swarm mode)
# secrets:
#   jwt_secret:
#     external: true
```

## Files in this directory

- Each file contains a single secret value (no trailing newline)
- File names should match the secret names used in docker-compose
- NEVER commit actual secret values - only .gitkeep placeholder

## Generating secure secrets

```bash
openssl rand -hex 32 > jwt_secret.txt
openssl rand -hex 32 > postgres_password.txt
```
