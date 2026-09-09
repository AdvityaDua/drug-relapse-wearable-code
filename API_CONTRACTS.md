# API Contracts

Complete API reference for the Drug Relapse Prevention Wearable Backend.

**Base URL**: `http://localhost:8000`

**Authentication**: All endpoints (unless marked 🔓 Public) require a JWT Bearer token:
```
Authorization: Bearer <access_token>
```

---

## Table of Contents

- [Authentication](#authentication)
  - [Register](#post-apiauthregister)
  - [Login](#post-apiauthlogin)
  - [Token Refresh](#post-apiauthtokenrefresh)
  - [Profile](#get-apiauthprofile)
  - [Forgot Password](#post-apiauthforgot-password)
  - [Verify OTP](#post-apiauthverify-otp)
  - [Reset Password](#post-apiauthreset-password)
- [Patient Management](#patient-management)
  - [List Patients](#get-apiauthpatients)
  - [Create Patient](#post-apiauthpatients)
  - [Get Patient](#get-apiauthpatientsid)
  - [Update Patient](#put-apiauthpatientsid)
  - [Delete Patient](#delete-apiauthpatientsid)
- [Data Collection](#data-collection)
  - [Create Collection Day](#post-apidatapatientspatient_iddays)
  - [List Collection Days](#get-apidatapatientspatient_iddays)
  - [Collection Day Detail](#get-apidatapatientspatient_iddaysid)
  - [Upload Sensor Readings](#post-apidatapatientspatient_idupload)
  - [Sync Sensor Readings](#get-apidatapatientspatient_idsync)
  - [Latest Sensor Reading](#get-apidatapatientspatient_idlatest)
- [Chatbot](#chatbot)
  - [Send Message](#post-apichatbotchat)
  - [List Conversations](#get-apichatbotconversations)
  - [Conversation Detail](#get-apichatbotconversationsid)
  - [Delete Conversation](#delete-apichatbotconversationsid)
- [Data Schemas](#data-schemas)
  - [User Object](#user-object)
  - [Patient Object](#patient-object)
  - [Collection Day Object](#collection-day-object)
  - [Sensor Reading Object](#sensor-reading-object)
  - [Conversation Object](#conversation-object)
  - [Message Object](#message-object)
- [Error Handling](#error-handling)
- [Enumerations](#enumerations)

---

## Authentication

### `POST /api/auth/register/`

🔓 **Public** — Create a new doctor account.

**Request Body**:
```json
{
  "email": "dr.smith@hospital.com",
  "password": "SecurePass123!",
  "password_confirm": "SecurePass123!",
  "first_name": "Dr. Jane",
  "last_name": "Smith"
}
```

| Field              | Type   | Required | Description                |
|--------------------|--------|----------|----------------------------|
| `email`            | string | ✅       | Unique email address       |
| `password`         | string | ✅       | Min 8 chars, Django-validated |
| `password_confirm` | string | ✅       | Must match `password`      |
| `first_name`       | string | ❌       | Doctor's first name        |
| `last_name`        | string | ❌       | Doctor's last name         |

**Response** `201 Created`:
```json
{
  "user": {
    "id": 1,
    "email": "dr.smith@hospital.com",
    "username": "dr.smith",
    "first_name": "Dr. Jane",
    "last_name": "Smith",
    "role": "doctor",
    "is_verified": false,
    "created_at": "2026-09-09T10:00:00Z",
    "updated_at": "2026-09-09T10:00:00Z"
  },
  "tokens": {
    "access": "eyJ0eXAiOiJKV1Qi...",
    "refresh": "eyJ0eXAiOiJKV1Qi..."
  }
}
```

**Errors**:
- `400` — Validation error (duplicate email, password mismatch, weak password)

---

### `POST /api/auth/login/`

🔓 **Public** — Authenticate and receive JWT tokens.

**Request Body**:
```json
{
  "email": "dr.smith@hospital.com",
  "password": "SecurePass123!"
}
```

**Response** `200 OK`:
```json
{
  "user": { "...": "same as register response" },
  "tokens": {
    "access": "eyJ0eXAiOiJKV1Qi...",
    "refresh": "eyJ0eXAiOiJKV1Qi..."
  }
}
```

**Errors**:
- `400` — Invalid credentials, account deactivated, or email not found

---

### `POST /api/auth/token/refresh/`

🔓 **Public** — Refresh an expired access token.

**Request Body**:
```json
{
  "refresh": "eyJ0eXAiOiJKV1Qi..."
}
```

**Response** `200 OK`:
```json
{
  "access": "eyJ0eXAiOiJKV1Qi...",
  "refresh": "eyJ0eXAiOiJKV1Qi..."
}
```

> **Note**: Refresh token rotation is enabled. Each refresh returns a new refresh token and the old one is invalidated.

---

### `GET /api/auth/profile/`

🔒 **Authenticated** — Retrieve the authenticated doctor's profile.

**Response** `200 OK`:
```json
{
  "id": 1,
  "email": "dr.smith@hospital.com",
  "username": "dr.smith",
  "first_name": "Dr. Jane",
  "last_name": "Smith",
  "role": "doctor",
  "is_verified": false,
  "created_at": "2026-09-09T10:00:00Z",
  "updated_at": "2026-09-09T10:00:00Z"
}
```

### `PUT /api/auth/profile/`

🔒 **Authenticated** — Update the authenticated doctor's profile (partial update).

**Request Body** (all fields optional):
```json
{
  "first_name": "Janet",
  "last_name": "Smith-Johnson"
}
```

| Field        | Type   | Writable | Description         |
|--------------|--------|----------|---------------------|
| `first_name` | string | ✅       | Doctor's first name |
| `last_name`  | string | ✅       | Doctor's last name  |
| `email`      | string | ❌       | Read-only           |
| `username`   | string | ❌       | Read-only           |
| `role`       | string | ❌       | Read-only           |

**Response** `200 OK`: Updated user object.

---

### `POST /api/auth/forgot-password/`

🔓 **Public** — Request a 6-digit OTP for password reset. The OTP is sent to the provided email and expires in 10 minutes.

**Request Body**:
```json
{
  "email": "dr.smith@hospital.com"
}
```

**Response** `200 OK`:
```json
{
  "message": "OTP sent to your email."
}
```

---

### `POST /api/auth/verify-otp/`

🔓 **Public** — Verify that an OTP code is valid (optional pre-check before resetting).

**Request Body**:
```json
{
  "email": "dr.smith@hospital.com",
  "code": "123456"
}
```

**Response** `200 OK`:
```json
{
  "message": "OTP verified successfully."
}
```

**Errors**:
- `400` — Invalid or expired OTP

---

### `POST /api/auth/reset-password/`

🔓 **Public** — Reset password using a valid OTP.

**Request Body**:
```json
{
  "email": "dr.smith@hospital.com",
  "code": "123456",
  "new_password": "NewSecurePass456!",
  "new_password_confirm": "NewSecurePass456!"
}
```

**Response** `200 OK`:
```json
{
  "message": "Password reset successfully."
}
```

**Errors**:
- `400` — Invalid/expired OTP, password mismatch, or weak password

---

## Patient Management

### `GET /api/auth/patients/`

🔒 **Authenticated** — List all patients managed by the authenticated doctor.

**Query Parameters**:

| Param              | Type   | Default | Description                                  |
|--------------------|--------|---------|----------------------------------------------|
| `include_inactive` | string | `false` | Set to `true` to include soft-deleted patients |

**Response** `200 OK`:
```json
[
  {
    "id": 1,
    "doctors": [1, 3],
    "name": "John Doe",
    "date_of_birth": "1990-05-15",
    "gender": "male",
    "device_id": "WEARABLE-001",
    "substance_type": "opioids",
    "diagnosis_notes": "OUD since 2020, currently in MAT program",
    "notes": "Weekly follow-up scheduled",
    "is_active": true,
    "created_at": "2026-09-09T10:00:00Z",
    "updated_at": "2026-09-09T10:00:00Z"
  }
]
```

---

### `POST /api/auth/patients/`

🔒 **Authenticated** — Create a new patient. The authenticated doctor is automatically added to the patient's doctor list.

**Request Body**:
```json
{
  "name": "John Doe",
  "date_of_birth": "1990-05-15",
  "gender": "male",
  "device_id": "WEARABLE-001",
  "substance_type": "opioids",
  "diagnosis_notes": "OUD since 2020, currently in MAT program",
  "notes": "Weekly follow-up scheduled"
}
```

| Field             | Type   | Required | Description                                    |
|-------------------|--------|----------|------------------------------------------------|
| `name`            | string | ✅       | Patient's full name                            |
| `date_of_birth`   | date   | ❌       | Format: `YYYY-MM-DD`                           |
| `gender`          | string | ❌       | See [Gender enum](#enumerations)               |
| `device_id`       | string | ❌       | Wearable device identifier                     |
| `substance_type`  | string | ❌       | See [Substance Type enum](#enumerations)       |
| `diagnosis_notes` | string | ❌       | Clinical diagnosis / medical history           |
| `notes`           | string | ❌       | General notes                                  |

**Response** `201 Created`: Full [Patient object](#patient-object).

---

### `GET /api/auth/patients/{id}/`

🔒 **Authenticated** — Retrieve a specific patient's details. Returns `404` if the patient does not belong to the doctor.

**Response** `200 OK`: Full [Patient object](#patient-object).

---

### `PUT /api/auth/patients/{id}/`

🔒 **Authenticated** — Update a patient's details (partial update supported). Only patients belonging to the doctor can be updated.

**Request Body** (all fields optional):
```json
{
  "substance_type": "polysubstance",
  "diagnosis_notes": "Updated diagnosis after recent evaluation"
}
```

**Response** `200 OK`: Updated [Patient object](#patient-object).

---

### `DELETE /api/auth/patients/{id}/`

🔒 **Authenticated** — **Soft-delete** a patient by setting `is_active = false`. The patient record and all associated data are preserved.

**Response** `200 OK`:
```json
{
  "message": "Patient deactivated."
}
```

> **Note**: To see deactivated patients, use `GET /api/auth/patients/?include_inactive=true`.

---

## Data Collection

All data endpoints are scoped under a patient: `/api/data/patients/{patient_id}/...`

The authenticated doctor must be in the patient's doctors list to access any data endpoint.

### `POST /api/data/patients/{patient_id}/days/`

🔒 **Authenticated** — Create a collection day for a patient. **Idempotent**: calling with the same date returns the existing day.

**Request Body**:
```json
{
  "date": "2026-09-01"
}
```

**Response** `201 Created` (new) / `200 OK` (existing):
```json
{
  "id": 1,
  "patient": 3,
  "date": "2026-09-01",
  "reading_count": 0,
  "created_at": "2026-09-09T10:00:00Z",
  "updated_at": "2026-09-09T10:00:00Z"
}
```

---

### `GET /api/data/patients/{patient_id}/days/`

🔒 **Authenticated** — List all collection days for a patient, ordered by most recent, annotated with reading counts.

**Response** `200 OK`:
```json
[
  {
    "id": 1,
    "patient": 3,
    "date": "2026-09-01",
    "reading_count": 42,
    "created_at": "2026-09-09T10:00:00Z",
    "updated_at": "2026-09-09T10:00:00Z"
  }
]
```

---

### `GET /api/data/patients/{patient_id}/days/{id}/`

🔒 **Authenticated** — Get details of a specific collection day, including summary stats.

**Response** `200 OK`:
```json
{
  "id": 1,
  "patient": 3,
  "date": "2026-09-01",
  "reading_count": 42,
  "first_reading_time": 1725148800,
  "last_reading_time": 1725235199,
  "created_at": "2026-09-09T10:00:00Z",
  "updated_at": "2026-09-09T10:00:00Z"
}
```

---

### `POST /api/data/patients/{patient_id}/upload/`

🔒 **Authenticated** — Bulk upload sensor readings for a patient. **Idempotent**: duplicate readings (same `collection_day` + `time`) are silently ignored.

**Request Body**:
```json
{
  "collection_day": 1,
  "readings": [
    {
      "time": 1725148800,
      "gsr": 512,
      "bodyTemp": 36.5,
      "hr": 72,
      "validHR": 1,
      "spo2": 98,
      "validSPO2": 1,
      "bno055_euler_heading": 180.5,
      "bno055_euler_roll": 0.3,
      "bno055_euler_pitch": -0.2
    },
    {
      "time": 1725148801,
      "gsr": 515,
      "bodyTemp": 36.6,
      "hr": 73,
      "validHR": 1,
      "spo2": 97,
      "validSPO2": 1
    }
  ]
}
```

| Field            | Type    | Required | Description                              |
|------------------|---------|----------|------------------------------------------|
| `collection_day` | integer | ✅       | ID of the DataCollectionDay              |
| `readings`       | array   | ✅       | Array of sensor reading objects (max 1000)|

Each reading **must** contain a `time` field. All other sensor fields are optional. See [Sensor Reading Object](#sensor-reading-object) for the full list of 35 sensor fields.

**Response** `201 Created`:
```json
{
  "message": "Successfully uploaded 2 readings.",
  "count": 2
}
```

**Errors**:
- `400` — Missing `time` field in a reading, or collection day doesn't belong to this patient
- `404` — Patient not found or not belonging to the doctor

---

### `GET /api/data/patients/{patient_id}/sync/`

🔒 **Authenticated** — Pull sensor readings for a patient. Paginated (100 per page).

**Query Parameters**:

| Param   | Type    | Default | Description                                 |
|---------|---------|---------|---------------------------------------------|
| `after` | integer | —       | Unix timestamp. Only readings after this time |
| `page`  | integer | `1`     | Page number for pagination                   |

**Response** `200 OK`:
```json
{
  "count": 250,
  "next": "http://localhost:8000/api/data/patients/3/sync/?after=1725148800&page=2",
  "previous": null,
  "results": [
    {
      "id": 1,
      "collection_day": 1,
      "time": 1725148800,
      "gsr": 512,
      "bodyTemp": 36.5,
      "hr": 72,
      "...": "all sensor fields"
    }
  ]
}
```

---

### `GET /api/data/patients/{patient_id}/latest/`

🔒 **Authenticated** — Get the most recent sensor reading for a patient. Useful for dashboard quick-glance display.

**Response** `200 OK`: Full [Sensor Reading Object](#sensor-reading-object).

**Errors**:
- `404` — No readings found or patient not found

---

## Chatbot

The AI chatbot is powered by a self-hosted Llama 70B model via a vLLM endpoint. Conversations are scoped per doctor.

### `POST /api/chatbot/chat/`

🔒 **Authenticated** — Send a message to the AI chatbot. Omit `conversation_id` to start a new conversation.

**Request Body** (new conversation):
```json
{
  "message": "What do elevated GSR readings mean for my patient?"
}
```

**Request Body** (continue conversation):
```json
{
  "message": "Tell me more about stress indicators.",
  "conversation_id": 5
}
```

| Field             | Type    | Required | Description                               |
|-------------------|---------|----------|-------------------------------------------|
| `message`         | string  | ✅       | Doctor's message (max 4000 chars)         |
| `conversation_id` | integer | ❌       | Existing conversation ID to continue      |

**Response** `200 OK`:
```json
{
  "conversation_id": 5,
  "reply": "Elevated GSR readings can indicate increased sympathetic nervous system activity, often associated with stress or emotional arousal...",
  "title": "What do elevated GSR readings mean for my patient?"
}
```

**Errors**:
- `404` — Conversation not found
- `503` — LLM service temporarily unavailable

---

### `GET /api/chatbot/conversations/`

🔒 **Authenticated** — List all conversations for the authenticated doctor, ordered by most recent. Paginated.

**Response** `200 OK`:
```json
{
  "count": 12,
  "next": null,
  "previous": null,
  "results": [
    {
      "id": 5,
      "title": "GSR Reading Analysis",
      "message_count": 8,
      "created_at": "2026-09-09T10:00:00Z",
      "updated_at": "2026-09-09T10:30:00Z"
    }
  ]
}
```

---

### `GET /api/chatbot/conversations/{id}/`

🔒 **Authenticated** — Retrieve a conversation with its full message history.

**Response** `200 OK`:
```json
{
  "id": 5,
  "title": "GSR Reading Analysis",
  "messages": [
    {
      "id": 1,
      "role": "user",
      "content": "What do elevated GSR readings mean?",
      "created_at": "2026-09-09T10:00:00Z"
    },
    {
      "id": 2,
      "role": "assistant",
      "content": "Elevated GSR readings can indicate...",
      "created_at": "2026-09-09T10:00:05Z"
    }
  ],
  "created_at": "2026-09-09T10:00:00Z",
  "updated_at": "2026-09-09T10:30:00Z"
}
```

---

### `DELETE /api/chatbot/conversations/{id}/`

🔒 **Authenticated** — Delete a conversation and all its messages (hard delete).

**Response** `204 No Content`

---

## Data Schemas

### User Object

Represents a doctor in the system.

| Field        | Type     | Description                        |
|--------------|----------|------------------------------------|
| `id`         | integer  | Unique identifier                  |
| `email`      | string   | Doctor's email (unique, login ID)  |
| `username`   | string   | Auto-generated from email          |
| `first_name` | string   | Doctor's first name                |
| `last_name`  | string   | Doctor's last name                 |
| `role`       | string   | Always `"doctor"`                  |
| `is_verified`| boolean  | Account verification status        |
| `created_at` | datetime | Account creation timestamp         |
| `updated_at` | datetime | Last profile update timestamp      |

---

### Patient Object

Represents a patient managed by one or more doctors.

| Field             | Type      | Description                                         |
|-------------------|-----------|-----------------------------------------------------|
| `id`              | integer   | Unique identifier                                   |
| `doctors`         | integer[] | List of doctor IDs managing this patient             |
| `name`            | string    | Patient's full name                                 |
| `date_of_birth`   | date      | `YYYY-MM-DD`, nullable                              |
| `gender`          | string    | See [Gender enum](#enumerations)                    |
| `device_id`       | string    | Wearable device identifier, nullable                |
| `substance_type`  | string    | See [Substance Type enum](#enumerations)            |
| `diagnosis_notes` | string    | Clinical diagnosis / medical history                |
| `notes`           | string    | General notes                                       |
| `is_active`       | boolean   | `false` = soft-deleted                              |
| `created_at`      | datetime  | Record creation timestamp                           |
| `updated_at`      | datetime  | Last update timestamp                               |

---

### Collection Day Object

Represents a single calendar day of data collection for a patient.

| Field               | Type     | Description                              |
|---------------------|----------|------------------------------------------|
| `id`                | integer  | Unique identifier                        |
| `patient`           | integer  | Patient ID                               |
| `date`              | date     | Calendar date (`YYYY-MM-DD`)             |
| `reading_count`     | integer  | Number of sensor readings on this day    |
| `first_reading_time`| integer  | Earliest reading timestamp (detail only) |
| `last_reading_time` | integer  | Latest reading timestamp (detail only)   |
| `created_at`        | datetime | Record creation timestamp                |
| `updated_at`        | datetime | Last update timestamp                    |

---

### Sensor Reading Object

A single timestamped reading from the wearable device containing up to 35 sensor fields.

| Field                   | Type    | Sensor                         | Description                        |
|-------------------------|---------|--------------------------------|------------------------------------|
| `id`                    | integer | —                              | Unique identifier                  |
| `collection_day`        | integer | —                              | Parent collection day ID           |
| `time`                  | integer | —                              | Unix epoch timestamp (seconds)     |
| **GSR** | | | |
| `gsr`                   | integer | TLA2022 ADC                    | Galvanic skin response raw value   |
| **Body Temperature** | | | |
| `bodyTemp`              | float   | MAX30205                       | Clinical body temp (°C)            |
| **Heart Rate & SpO2** | | | |
| `hr`                    | integer | MAX30102                       | Heart rate (bpm)                   |
| `validHR`               | integer | MAX30102                       | 1 = reliable, 0 = unreliable      |
| `spo2`                  | integer | MAX30102                       | Blood oxygen saturation (%)        |
| `validSPO2`             | integer | MAX30102                       | 1 = reliable, 0 = unreliable      |
| **BNO055 Euler Angles** | | | |
| `bno055_euler_heading`  | float   | BNO055 IMU                     | Yaw in degrees                     |
| `bno055_euler_roll`     | float   | BNO055 IMU                     | Roll in degrees                    |
| `bno055_euler_pitch`    | float   | BNO055 IMU                     | Pitch in degrees                   |
| **BNO055 Quaternion** | | | |
| `bno055_quat_w`         | float   | BNO055 IMU                     | Quaternion W                       |
| `bno055_quat_x`         | float   | BNO055 IMU                     | Quaternion X                       |
| `bno055_quat_y`         | float   | BNO055 IMU                     | Quaternion Y                       |
| `bno055_quat_z`         | float   | BNO055 IMU                     | Quaternion Z                       |
| **BNO055 Linear Accel** | | | |
| `bno055_linear_x`       | float   | BNO055 IMU                     | Linear accel X (m/s²), no gravity  |
| `bno055_linear_y`       | float   | BNO055 IMU                     | Linear accel Y (m/s²), no gravity  |
| `bno055_linear_z`       | float   | BNO055 IMU                     | Linear accel Z (m/s²), no gravity  |
| **BNO055 Gravity** | | | |
| `bno055_gravity_x`      | float   | BNO055 IMU                     | Gravity vector X (m/s²)            |
| `bno055_gravity_y`      | float   | BNO055 IMU                     | Gravity vector Y (m/s²)            |
| `bno055_gravity_z`      | float   | BNO055 IMU                     | Gravity vector Z (m/s²)            |
| **BNO055 Raw Accel** | | | |
| `bno055_accel_x`        | float   | BNO055 IMU                     | Raw accel X (m/s²), with gravity   |
| `bno055_accel_y`        | float   | BNO055 IMU                     | Raw accel Y (m/s²), with gravity   |
| `bno055_accel_z`        | float   | BNO055 IMU                     | Raw accel Z (m/s²), with gravity   |
| **BNO055 Gyroscope** | | | |
| `bno055_gyro_x`         | float   | BNO055 IMU                     | Angular velocity X (°/s)           |
| `bno055_gyro_y`         | float   | BNO055 IMU                     | Angular velocity Y (°/s)           |
| `bno055_gyro_z`         | float   | BNO055 IMU                     | Angular velocity Z (°/s)           |
| **BNO055 Magnetometer** | | | |
| `bno055_mag_x`          | float   | BNO055 IMU                     | Magnetic field X (µT)              |
| `bno055_mag_y`          | float   | BNO055 IMU                     | Magnetic field Y (µT)              |
| `bno055_mag_z`          | float   | BNO055 IMU                     | Magnetic field Z (µT)              |
| **BNO055 Calibration** | | | |
| `bno055_temp`           | integer | BNO055 IMU                     | Chip temperature (°C)              |
| `bno055_calib_sys`      | integer | BNO055 IMU                     | System calibration (0-3)           |
| `bno055_calib_gyro`     | integer | BNO055 IMU                     | Gyro calibration (0-3)             |
| `bno055_calib_accel`    | integer | BNO055 IMU                     | Accel calibration (0-3)            |
| `bno055_calib_mag`      | integer | BNO055 IMU                     | Mag calibration (0-3)              |
| **Metadata** | | | |
| `synced_at`             | datetime| —                              | Server-side sync timestamp         |

> All sensor fields except `time` are optional and nullable.

---

### Conversation Object

| Field           | Type     | Description                       |
|-----------------|----------|-----------------------------------|
| `id`            | integer  | Unique identifier                 |
| `title`         | string   | Auto-generated from first message |
| `message_count` | integer  | Number of messages (list only)    |
| `messages`      | array    | Full message history (detail only)|
| `created_at`    | datetime | Conversation start timestamp      |
| `updated_at`    | datetime | Last activity timestamp           |

---

### Message Object

| Field       | Type     | Description                              |
|-------------|----------|------------------------------------------|
| `id`        | integer  | Unique identifier                        |
| `role`      | string   | `"user"`, `"assistant"`, or `"system"`   |
| `content`   | string   | Message text content                     |
| `created_at`| datetime | Message timestamp                        |

---

## Error Handling

All error responses follow a consistent structure:

**Validation Errors** (`400 Bad Request`):
```json
{
  "field_name": ["Error message."]
}
```

**Authentication Error** (`401 Unauthorized`):
```json
{
  "detail": "Authentication credentials were not provided."
}
```

**Not Found** (`404 Not Found`):
```json
{
  "message": "Resource not found."
}
```

**Service Unavailable** (`503`):
```json
{
  "message": "Chatbot is temporarily unavailable. Please try again later."
}
```

---

## Enumerations

### Gender

| Value              | Label              |
|--------------------|--------------------|
| `male`             | Male               |
| `female`           | Female             |
| `other`            | Other              |
| `prefer_not_to_say`| Prefer not to say  |

### Substance Type

| Value              | Label              |
|--------------------|--------------------|
| `alcohol`          | Alcohol            |
| `opioids`          | Opioids            |
| `cannabis`         | Cannabis           |
| `stimulants`       | Stimulants         |
| `benzodiazepines`  | Benzodiazepines    |
| `tobacco`          | Tobacco            |
| `polysubstance`    | Polysubstance      |
| `other`            | Other              |

### Message Role

| Value       | Description                        |
|-------------|------------------------------------|
| `user`      | Message sent by the doctor         |
| `assistant` | Response from the AI chatbot       |
| `system`    | System prompt (internal, not exposed)|

---

## JWT Token Details

| Setting                 | Value   |
|-------------------------|---------|
| Access token lifetime   | 7 days  |
| Refresh token lifetime  | 30 days |
| Refresh token rotation  | Enabled |
| Algorithm               | HS256   |

---

## Pagination

All list endpoints that return large datasets use **page-number pagination**:

| Parameter | Default | Description          |
|-----------|---------|----------------------|
| `page`    | `1`     | Page number          |
| Page size | `100`   | Results per page     |

**Response shape**:
```json
{
  "count": 250,
  "next": "http://localhost:8000/api/...?page=3",
  "previous": "http://localhost:8000/api/...?page=1",
  "results": [ "..." ]
}
```

Endpoints using pagination: `/sync/`, `/conversations/`.
