import time
from django.test import TestCase
from django.contrib.auth import get_user_model
from rest_framework.test import APIClient
from rest_framework import status

from users.models import Patient
from .models import DataCollectionDay, SensorReading

User = get_user_model()


class DataCollectionDayTests(TestCase):
    """Test collection day creation and listing for a patient."""

    def setUp(self):
        self.client = APIClient()
        self.doctor = User.objects.create_user(
            username='drsmith',
            email='dr.smith@hospital.com',
            password='SecurePass123!',
        )
        self.patient = Patient.objects.create(
            name='John Doe',
            device_id='WEARABLE-001',
        )
        self.patient.doctors.add(self.doctor)
        self.client.force_authenticate(user=self.doctor)

        self.days_url = f'/api/data/patients/{self.patient.pk}/days/'

    def test_create_collection_day(self):
        """Doctor can create a collection day for their patient."""
        data = {'date': '2026-08-20'}
        response = self.client.post(self.days_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data['date'], '2026-08-20')
        self.assertEqual(response.data['patient'], self.patient.pk)
        self.assertEqual(response.data['reading_count'], 0)

    def test_create_collection_day_idempotent(self):
        """Creating the same day twice returns 200 (not 201) — idempotent."""
        data = {'date': '2026-08-20'}
        resp1 = self.client.post(self.days_url, data, format='json')
        self.assertEqual(resp1.status_code, status.HTTP_201_CREATED)

        resp2 = self.client.post(self.days_url, data, format='json')
        self.assertEqual(resp2.status_code, status.HTTP_200_OK)
        self.assertEqual(resp1.data['id'], resp2.data['id'])

    def test_list_collection_days(self):
        """Doctor can list collection days for their patient."""
        DataCollectionDay.objects.create(patient=self.patient, date='2026-08-20')
        DataCollectionDay.objects.create(patient=self.patient, date='2026-08-21')

        response = self.client.get(self.days_url)
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(len(response.data), 2)

    def test_collection_day_detail(self):
        """Doctor can get details of a specific collection day."""
        day = DataCollectionDay.objects.create(patient=self.patient, date='2026-08-20')
        detail_url = f'{self.days_url}{day.pk}/'

        response = self.client.get(detail_url)
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['date'], '2026-08-20')
        self.assertEqual(response.data['reading_count'], 0)

    def test_cannot_access_other_doctors_patient_days(self):
        """Doctor cannot create/list days for another doctor's patient."""
        other_doctor = User.objects.create_user(
            username='drjones',
            email='dr.jones@hospital.com',
            password='SecurePass123!',
        )
        other_patient = Patient.objects.create(name='Not My Patient')
        other_patient.doctors.add(other_doctor)

        url = f'/api/data/patients/{other_patient.pk}/days/'
        response = self.client.get(url)
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)

    def test_nonexistent_patient(self):
        """Requesting days for a non-existent patient returns 404."""
        url = '/api/data/patients/99999/days/'
        response = self.client.get(url)
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)


class SensorUploadTests(TestCase):
    """Test bulk sensor data upload for a patient."""

    def setUp(self):
        self.client = APIClient()
        self.doctor = User.objects.create_user(
            username='drsmith',
            email='dr.smith@hospital.com',
            password='SecurePass123!',
        )
        self.patient = Patient.objects.create(
            name='John Doe',
            device_id='WEARABLE-001',
        )
        self.patient.doctors.add(self.doctor)
        self.day = DataCollectionDay.objects.create(
            patient=self.patient,
            date='2026-08-20',
        )
        self.client.force_authenticate(user=self.doctor)

        self.upload_url = f'/api/data/patients/{self.patient.pk}/upload/'

    def test_upload_sensor_readings(self):
        """Doctor can upload sensor readings for their patient."""
        data = {
            'collection_day': self.day.pk,
            'readings': [
                {
                    'time': 1724150400,
                    'gsr': 512,
                    'hr': 72,
                    'bodyTemp': 36.5,
                    'spo2': 98,
                    'validHR': 1,
                    'validSPO2': 1,
                },
                {
                    'time': 1724150401,
                    'gsr': 515,
                    'hr': 73,
                    'bodyTemp': 36.6,
                    'spo2': 97,
                    'validHR': 1,
                    'validSPO2': 1,
                },
            ],
        }
        response = self.client.post(self.upload_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data['count'], 2)
        self.assertEqual(SensorReading.objects.count(), 2)

    def test_upload_idempotent(self):
        """Re-uploading same readings does not create duplicates."""
        data = {
            'collection_day': self.day.pk,
            'readings': [
                {'time': 1724150400, 'gsr': 512, 'hr': 72},
            ],
        }
        self.client.post(self.upload_url, data, format='json')
        self.client.post(self.upload_url, data, format='json')
        self.assertEqual(SensorReading.objects.count(), 1)

    def test_upload_missing_time_field(self):
        """Upload fails if a reading is missing the 'time' field."""
        data = {
            'collection_day': self.day.pk,
            'readings': [
                {'gsr': 512, 'hr': 72},
            ],
        }
        response = self.client.post(self.upload_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_upload_wrong_patient_collection_day(self):
        """Upload fails if collection_day belongs to a different patient."""
        other_patient = Patient.objects.create(name='Other')
        other_patient.doctors.add(self.doctor)
        other_day = DataCollectionDay.objects.create(
            patient=other_patient,
            date='2026-08-20',
        )

        data = {
            'collection_day': other_day.pk,
            'readings': [{'time': 1724150400}],
        }
        response = self.client.post(self.upload_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_upload_other_doctors_patient(self):
        """Doctor cannot upload data to another doctor's patient."""
        other_doctor = User.objects.create_user(
            username='drjones',
            email='dr.jones@hospital.com',
            password='SecurePass123!',
        )
        other_patient = Patient.objects.create(name='Not Mine')
        other_patient.doctors.add(other_doctor)

        url = f'/api/data/patients/{other_patient.pk}/upload/'
        data = {
            'collection_day': self.day.pk,
            'readings': [{'time': 1724150400}],
        }
        response = self.client.post(url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)


class SensorSyncAndLatestTests(TestCase):
    """Test sensor data sync and latest reading endpoints."""

    def setUp(self):
        self.client = APIClient()
        self.doctor = User.objects.create_user(
            username='drsmith',
            email='dr.smith@hospital.com',
            password='SecurePass123!',
        )
        self.patient = Patient.objects.create(
            name='John Doe',
            device_id='WEARABLE-001',
        )
        self.patient.doctors.add(self.doctor)
        self.day = DataCollectionDay.objects.create(
            patient=self.patient,
            date='2026-08-20',
        )
        # Create some readings
        SensorReading.objects.create(
            collection_day=self.day, time=1724150400, gsr=512, hr=72,
        )
        SensorReading.objects.create(
            collection_day=self.day, time=1724150401, gsr=515, hr=73,
        )
        SensorReading.objects.create(
            collection_day=self.day, time=1724150402, gsr=520, hr=75,
        )

        self.client.force_authenticate(user=self.doctor)
        self.sync_url = f'/api/data/patients/{self.patient.pk}/sync/'
        self.latest_url = f'/api/data/patients/{self.patient.pk}/latest/'

    def test_sync_all(self):
        """Sync without 'after' param returns all readings."""
        response = self.client.get(self.sync_url)
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['count'], 3)

    def test_sync_after_timestamp(self):
        """Sync with 'after' param returns only newer readings."""
        response = self.client.get(f'{self.sync_url}?after=1724150400')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['count'], 2)

    def test_sync_invalid_after(self):
        """Sync with invalid 'after' param returns all readings (graceful)."""
        response = self.client.get(f'{self.sync_url}?after=not-a-number')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['count'], 3)

    def test_latest_reading(self):
        """Latest endpoint returns the most recent reading."""
        response = self.client.get(self.latest_url)
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['time'], 1724150402)
        self.assertEqual(response.data['hr'], 75)

    def test_latest_no_readings(self):
        """Latest returns 404 when no readings exist for the patient."""
        empty_patient = Patient.objects.create(name='Empty')
        empty_patient.doctors.add(self.doctor)
        url = f'/api/data/patients/{empty_patient.pk}/latest/'
        response = self.client.get(url)
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)

    def test_sync_other_doctors_patient(self):
        """Doctor cannot sync data from another doctor's patient."""
        other_doctor = User.objects.create_user(
            username='drjones',
            email='dr.jones@hospital.com',
            password='SecurePass123!',
        )
        other_patient = Patient.objects.create(name='Not Mine')
        other_patient.doctors.add(other_doctor)

        url = f'/api/data/patients/{other_patient.pk}/sync/'
        response = self.client.get(url)
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        # Should return empty results since the patient doesn't belong to this doctor
        self.assertEqual(response.data['count'], 0)


class EndToEndWorkflowTest(TestCase):
    """
    Full workflow integration test:
    Register doctor → Create patient → Create collection day →
    Upload sensor data → Verify sync and latest → Soft-delete patient
    """

    def setUp(self):
        self.client = APIClient()

    def test_full_workflow(self):
        # ── Step 1: Register a doctor ─────────────────────────────────
        register_resp = self.client.post('/api/auth/register/', {
            'email': 'dr.workflow@hospital.com',
            'password': 'WorkflowPass123!',
            'password_confirm': 'WorkflowPass123!',
            'first_name': 'Dr. Workflow',
            'last_name': 'Test',
        }, format='json')
        self.assertEqual(register_resp.status_code, status.HTTP_201_CREATED)
        access_token = register_resp.data['tokens']['access']

        # Authenticate with the JWT token
        self.client.credentials(HTTP_AUTHORIZATION=f'Bearer {access_token}')

        # ── Step 2: Create a patient ──────────────────────────────────
        patient_resp = self.client.post('/api/auth/patients/', {
            'name': 'Workflow Patient',
            'date_of_birth': '1985-03-22',
            'gender': 'female',
            'device_id': 'WRK-DEVICE-001',
            'substance_type': 'alcohol',
            'diagnosis_notes': 'AUD for 5 years',
        }, format='json')
        self.assertEqual(patient_resp.status_code, status.HTTP_201_CREATED)
        patient_id = patient_resp.data['id']
        self.assertEqual(patient_resp.data['name'], 'Workflow Patient')
        self.assertEqual(patient_resp.data['substance_type'], 'alcohol')

        # ── Step 3: Verify patient appears in list ────────────────────
        list_resp = self.client.get('/api/auth/patients/')
        self.assertEqual(list_resp.status_code, status.HTTP_200_OK)
        self.assertEqual(len(list_resp.data), 1)

        # ── Step 4: Create a collection day ───────────────────────────
        day_resp = self.client.post(
            f'/api/data/patients/{patient_id}/days/',
            {'date': '2026-09-01'},
            format='json',
        )
        self.assertEqual(day_resp.status_code, status.HTTP_201_CREATED)
        day_id = day_resp.data['id']

        # ── Step 5: Upload sensor readings ────────────────────────────
        readings = [
            {
                'time': 1725148800 + i,
                'gsr': 500 + i,
                'hr': 70 + i,
                'bodyTemp': 36.5 + (i * 0.1),
                'spo2': 98,
                'validHR': 1,
                'validSPO2': 1,
                'bno055_euler_heading': 180.0 + i,
                'bno055_euler_roll': 0.5,
                'bno055_euler_pitch': -0.3,
            }
            for i in range(5)
        ]
        upload_resp = self.client.post(
            f'/api/data/patients/{patient_id}/upload/',
            {'collection_day': day_id, 'readings': readings},
            format='json',
        )
        self.assertEqual(upload_resp.status_code, status.HTTP_201_CREATED)
        self.assertEqual(upload_resp.data['count'], 5)

        # ── Step 6: Verify collection day detail shows reading count ──
        detail_resp = self.client.get(
            f'/api/data/patients/{patient_id}/days/{day_id}/'
        )
        self.assertEqual(detail_resp.status_code, status.HTTP_200_OK)
        self.assertEqual(detail_resp.data['reading_count'], 5)

        # ── Step 7: Sync readings ─────────────────────────────────────
        sync_resp = self.client.get(
            f'/api/data/patients/{patient_id}/sync/'
        )
        self.assertEqual(sync_resp.status_code, status.HTTP_200_OK)
        self.assertEqual(sync_resp.data['count'], 5)

        # ── Step 8: Get latest reading ────────────────────────────────
        latest_resp = self.client.get(
            f'/api/data/patients/{patient_id}/latest/'
        )
        self.assertEqual(latest_resp.status_code, status.HTTP_200_OK)
        self.assertEqual(latest_resp.data['time'], 1725148804)
        self.assertEqual(latest_resp.data['hr'], 74)

        # ── Step 9: Update patient ────────────────────────────────────
        update_resp = self.client.put(
            f'/api/auth/patients/{patient_id}/',
            {'substance_type': 'polysubstance', 'notes': 'Updated during workflow test'},
            format='json',
        )
        self.assertEqual(update_resp.status_code, status.HTTP_200_OK)
        self.assertEqual(update_resp.data['substance_type'], 'polysubstance')

        # ── Step 10: Soft-delete patient ──────────────────────────────
        delete_resp = self.client.delete(f'/api/auth/patients/{patient_id}/')
        self.assertEqual(delete_resp.status_code, status.HTTP_200_OK)

        # Patient should be hidden from default list
        list_after = self.client.get('/api/auth/patients/')
        self.assertEqual(len(list_after.data), 0)

        # But visible with include_inactive
        list_all = self.client.get('/api/auth/patients/?include_inactive=true')
        self.assertEqual(len(list_all.data), 1)
        self.assertFalse(list_all.data[0]['is_active'])
