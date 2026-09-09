# Drug Relapse Prevention — Wearable Backend

A Django REST API backend for a drug relapse prevention and monitoring platform. The system follows a **doctor-centric architecture** where doctors are the primary users who manage patients and collect wearable sensor data on their behalf.

## Architecture Overview

```
┌─────────────────────────────────────────────────────────┐
│                     Mobile App (Flutter)                 │
│              (Doctor authenticates & operates)           │
└────────────┬──────────────┬──────────────┬──────────────┘
             │              │              │
      ┌──────▼──────┐ ┌────▼────┐ ┌───────▼───────┐
      │  Auth API   │ │Data API │ │  Chatbot API  │
      │ /api/auth/  │ │/api/data│ │ /api/chatbot/ │
      └──────┬──────┘ └────┬────┘ └───────┬───────┘
             │              │              │
      ┌──────▼──────────────▼──────┐ ┌─────▼──────┐
      │     PostgreSQL (Neon)      │ │  vLLM API  │
      │  Users, Patients, Sensors  │ │ (Llama 70B)│
      └────────────────────────────┘ └────────────┘
```

## Tech Stack

| Layer          | Technology                              |
|----------------|-----------------------------------------|
| Framework      | Django 6.1 + Django REST Framework      |
| Auth           | JWT via `djangorestframework-simplejwt`  |
| Database       | PostgreSQL (Neon serverless)             |
| LLM            | Llama 70B via self-hosted vLLM endpoint |
| Package Mgr    | `uv` (with `pyproject.toml`)            |
| Python         | 3.12+                                   |

## Project Structure

```
backend/
├── backend/              # Django project settings
│   ├── settings.py
│   ├── urls.py           # Root URL routing
│   ├── wsgi.py
│   └── asgi.py
├── users/                # Auth & patient management
│   ├── models.py         # User (Doctor), Patient, PasswordResetOTP
│   ├── serializers.py
│   ├── views.py
│   ├── urls.py
│   └── tests.py
├── data/                 # Wearable sensor data collection
│   ├── models.py         # DataCollectionDay, SensorReading (35 sensor fields)
│   ├── serializers.py
│   ├── views.py
│   ├── urls.py
│   └── tests.py
├── chatbot/              # AI wellness assistant
│   ├── models.py         # Conversation, Message
│   ├── nvidia_client.py  # vLLM client wrapper
│   ├── serializers.py
│   ├── views.py
│   └── urls.py
├── manage.py
├── pyproject.toml
└── .env                  # Environment variables (not committed)
```

## Quick Start

### Prerequisites

- Python 3.12+
- [`uv`](https://docs.astral.sh/uv/) package manager
- PostgreSQL database (or Neon account)

### Setup

```bash
# Clone and navigate
cd backend/

# Install dependencies
uv sync

# Configure environment variables
cp .env.example .env
# Edit .env with your DATABASE_URL and LLM credentials

# Run migrations
uv run python manage.py migrate

# Start development server
uv run python manage.py runserver
```

### Environment Variables

| Variable        | Description                          | Example                                |
|-----------------|--------------------------------------|----------------------------------------|
| `DATABASE_URL`  | PostgreSQL connection string         | `postgresql://user:pass@host/dbname`   |
| `LLM_BASE_URL`  | vLLM-compatible endpoint URL         | `https://your-vllm-host/llm/v1`       |
| `LLM_MODEL_NAME`| Model name on the vLLM server        | `llama70b`                             |
| `LLM_API_KEY`   | API key for the LLM endpoint         | `not-needed` (for self-hosted)         |

### Running Tests

```bash
uv run python manage.py test users data -v 2
```

## Data Model

```
┌──────────┐       M:N        ┌───────────┐
│   User   │◄────────────────►│  Patient   │
│ (Doctor) │   doctors M2M    │            │
└──────────┘                  └─────┬──────┘
     │                              │ 1:N
     │                        ┌─────▼──────────────┐
     │                        │ DataCollectionDay   │
     │                        │ (patient + date)    │
     │                        └─────┬──────────────┘
     │                              │ 1:N
     │                        ┌─────▼──────────────┐
     │                        │   SensorReading     │
     │                        │  (35 sensor fields) │
     │                        └────────────────────┘
     │
     │ 1:N                    1:N
     ├───────────────► Conversation ──────► Message
     │
     └───────────────► PasswordResetOTP
```

## API Documentation

See **[API_CONTRACTS.md](./API_CONTRACTS.md)** for the full API reference with request/response schemas for every endpoint.

## Key Design Decisions

- **Doctor-centric**: Only doctors register and authenticate. Patients are non-auth data records managed by their doctors.
- **Many-to-many**: A patient can be shared across multiple doctors for collaborative care.
- **Soft-delete**: Patients are never hard-deleted — `DELETE` sets `is_active=False` to preserve data integrity.
- **Idempotent syncs**: Collection days use `get_or_create`, sensor uploads use `ignore_conflicts` — safe to re-sync.
- **JWT auth**: 7-day access tokens, 30-day refresh tokens with rotation.
