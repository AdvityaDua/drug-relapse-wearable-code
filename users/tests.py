import time
from django.test import TestCase
from django.contrib.auth import get_user_model
from rest_framework.test import APIClient
from rest_framework import status

from .models import Patient, PasswordResetOTP

User = get_user_model()


class DoctorRegistrationTests(TestCase):
    """Test doctor registration flow."""

    def setUp(self):
        self.client = APIClient()
        self.register_url = '/api/auth/register/'

    def test_register_doctor_success(self):
        """A doctor can register with email and password."""
        data = {
            'email': 'dr.smith@hospital.com',
            'password': 'SecurePass123!',
            'password_confirm': 'SecurePass123!',
            'first_name': 'Dr. Jane',
            'last_name': 'Smith',
        }
        response = self.client.post(self.register_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertIn('user', response.data)
        self.assertIn('tokens', response.data)
        self.assertEqual(response.data['user']['email'], 'dr.smith@hospital.com')
        self.assertEqual(response.data['user']['role'], 'doctor')
        self.assertIn('access', response.data['tokens'])
        self.assertIn('refresh', response.data['tokens'])

    def test_register_password_mismatch(self):
        """Registration fails when passwords don't match."""
        data = {
            'email': 'dr.jones@hospital.com',
            'password': 'SecurePass123!',
            'password_confirm': 'DifferentPass456!',
        }
        response = self.client.post(self.register_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_register_duplicate_email(self):
        """Registration fails with a duplicate email."""
        data = {
            'email': 'dr.smith@hospital.com',
            'password': 'SecurePass123!',
            'password_confirm': 'SecurePass123!',
        }
        self.client.post(self.register_url, data, format='json')
        response = self.client.post(self.register_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_register_no_patient_fields(self):
        """Registration response should NOT contain patient-level fields."""
        data = {
            'email': 'dr.new@hospital.com',
            'password': 'SecurePass123!',
            'password_confirm': 'SecurePass123!',
        }
        response = self.client.post(self.register_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        user_data = response.data['user']
        self.assertNotIn('device_id', user_data)
        self.assertNotIn('date_of_birth', user_data)
        self.assertNotIn('profile_image', user_data)


class DoctorLoginTests(TestCase):
    """Test doctor login flow."""

    def setUp(self):
        self.client = APIClient()
        self.login_url = '/api/auth/login/'
        # Create a doctor
        self.user = User.objects.create_user(
            username='drsmith',
            email='dr.smith@hospital.com',
            password='SecurePass123!',
        )

    def test_login_success(self):
        """A doctor can log in with correct credentials."""
        data = {
            'email': 'dr.smith@hospital.com',
            'password': 'SecurePass123!',
        }
        response = self.client.post(self.login_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertIn('tokens', response.data)
        self.assertIn('user', response.data)

    def test_login_wrong_password(self):
        """Login fails with wrong password."""
        data = {
            'email': 'dr.smith@hospital.com',
            'password': 'WrongPassword!',
        }
        response = self.client.post(self.login_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)

    def test_login_nonexistent_email(self):
        """Login fails with non-existent email."""
        data = {
            'email': 'nonexistent@hospital.com',
            'password': 'SecurePass123!',
        }
        response = self.client.post(self.login_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_400_BAD_REQUEST)


class DoctorProfileTests(TestCase):
    """Test doctor profile retrieval and update."""

    def setUp(self):
        self.client = APIClient()
        self.profile_url = '/api/auth/profile/'
        self.user = User.objects.create_user(
            username='drsmith',
            email='dr.smith@hospital.com',
            password='SecurePass123!',
            first_name='Jane',
            last_name='Smith',
        )
        self.client.force_authenticate(user=self.user)

    def test_get_profile(self):
        """Doctor can retrieve their profile."""
        response = self.client.get(self.profile_url)
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['email'], 'dr.smith@hospital.com')
        self.assertEqual(response.data['role'], 'doctor')
        self.assertNotIn('device_id', response.data)
        self.assertNotIn('date_of_birth', response.data)

    def test_update_profile(self):
        """Doctor can update their name."""
        data = {'first_name': 'Janet'}
        response = self.client.put(self.profile_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['first_name'], 'Janet')

    def test_profile_unauthenticated(self):
        """Profile endpoint requires authentication."""
        self.client.force_authenticate(user=None)
        response = self.client.get(self.profile_url)
        self.assertEqual(response.status_code, status.HTTP_401_UNAUTHORIZED)


class PatientCRUDTests(TestCase):
    """Test full patient CRUD operations by a doctor."""

    def setUp(self):
        self.client = APIClient()
        self.patients_url = '/api/auth/patients/'
        self.doctor = User.objects.create_user(
            username='drsmith',
            email='dr.smith@hospital.com',
            password='SecurePass123!',
        )
        self.other_doctor = User.objects.create_user(
            username='drjones',
            email='dr.jones@hospital.com',
            password='SecurePass123!',
        )
        self.client.force_authenticate(user=self.doctor)

    def test_create_patient(self):
        """Doctor can create a patient with full profile fields."""
        data = {
            'name': 'John Doe',
            'date_of_birth': '1990-05-15',
            'gender': 'male',
            'device_id': 'WEARABLE-001',
            'substance_type': 'opioids',
            'diagnosis_notes': 'OUD since 2020, currently in MAT program',
            'notes': 'Weekly follow-up scheduled',
        }
        response = self.client.post(self.patients_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data['name'], 'John Doe')
        self.assertEqual(response.data['gender'], 'male')
        self.assertEqual(response.data['substance_type'], 'opioids')
        # Doctor should be auto-assigned
        self.assertIn(self.doctor.pk, response.data['doctors'])

    def test_create_patient_minimal(self):
        """Doctor can create a patient with only the required name field."""
        data = {'name': 'Jane Roe'}
        response = self.client.post(self.patients_url, data, format='json')
        self.assertEqual(response.status_code, status.HTTP_201_CREATED)
        self.assertEqual(response.data['name'], 'Jane Roe')
        self.assertTrue(response.data['is_active'])

    def test_list_patients(self):
        """Doctor can list their patients."""
        # Create two patients
        Patient.objects.create(name='Patient A').doctors.add(self.doctor)
        Patient.objects.create(name='Patient B').doctors.add(self.doctor)
        # Create a patient for another doctor (should NOT appear)
        Patient.objects.create(name='Patient C').doctors.add(self.other_doctor)

        response = self.client.get(self.patients_url)
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(len(response.data), 2)
        names = {p['name'] for p in response.data}
        self.assertEqual(names, {'Patient A', 'Patient B'})

    def test_get_patient_detail(self):
        """Doctor can retrieve a specific patient's details."""
        patient = Patient.objects.create(
            name='John Doe',
            gender='male',
            substance_type='alcohol',
        )
        patient.doctors.add(self.doctor)

        response = self.client.get(f'{self.patients_url}{patient.pk}/')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['name'], 'John Doe')
        self.assertEqual(response.data['substance_type'], 'alcohol')

    def test_update_patient(self):
        """Doctor can update a patient's details."""
        patient = Patient.objects.create(name='John Doe')
        patient.doctors.add(self.doctor)

        data = {
            'substance_type': 'cannabis',
            'diagnosis_notes': 'Updated diagnosis',
        }
        response = self.client.put(f'{self.patients_url}{patient.pk}/', data, format='json')
        self.assertEqual(response.status_code, status.HTTP_200_OK)
        self.assertEqual(response.data['substance_type'], 'cannabis')
        self.assertEqual(response.data['diagnosis_notes'], 'Updated diagnosis')

    def test_soft_delete_patient(self):
        """DELETE soft-deletes a patient (sets is_active=False)."""
        patient = Patient.objects.create(name='John Doe')
        patient.doctors.add(self.doctor)

        response = self.client.delete(f'{self.patients_url}{patient.pk}/')
        self.assertEqual(response.status_code, status.HTTP_200_OK)

        # Patient should still exist in DB but be inactive
        patient.refresh_from_db()
        self.assertFalse(patient.is_active)

    def test_soft_deleted_hidden_by_default(self):
        """Soft-deleted patients are hidden from default list."""
        active = Patient.objects.create(name='Active')
        active.doctors.add(self.doctor)
        inactive = Patient.objects.create(name='Inactive', is_active=False)
        inactive.doctors.add(self.doctor)

        response = self.client.get(self.patients_url)
        self.assertEqual(len(response.data), 1)
        self.assertEqual(response.data[0]['name'], 'Active')

    def test_include_inactive_param(self):
        """?include_inactive=true shows all patients including soft-deleted."""
        active = Patient.objects.create(name='Active')
        active.doctors.add(self.doctor)
        inactive = Patient.objects.create(name='Inactive', is_active=False)
        inactive.doctors.add(self.doctor)

        response = self.client.get(f'{self.patients_url}?include_inactive=true')
        self.assertEqual(len(response.data), 2)

    def test_cannot_access_other_doctors_patient(self):
        """Doctor cannot access a patient that belongs to another doctor."""
        patient = Patient.objects.create(name='Not My Patient')
        patient.doctors.add(self.other_doctor)

        response = self.client.get(f'{self.patients_url}{patient.pk}/')
        self.assertEqual(response.status_code, status.HTTP_404_NOT_FOUND)

    def test_multi_doctor_patient(self):
        """A patient can belong to multiple doctors."""
        patient = Patient.objects.create(name='Shared Patient')
        patient.doctors.add(self.doctor, self.other_doctor)

        # Both doctors should be able to see the patient
        response = self.client.get(f'{self.patients_url}{patient.pk}/')
        self.assertEqual(response.status_code, status.HTTP_200_OK)

        self.client.force_authenticate(user=self.other_doctor)
        response = self.client.get(f'{self.patients_url}{patient.pk}/')
        self.assertEqual(response.status_code, status.HTTP_200_OK)

    def test_unauthenticated_access(self):
        """Patient endpoints require authentication."""
        self.client.force_authenticate(user=None)
        response = self.client.get(self.patients_url)
        self.assertEqual(response.status_code, status.HTTP_401_UNAUTHORIZED)
