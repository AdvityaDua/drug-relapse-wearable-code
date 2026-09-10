from django.db.models import Count, Min, Max
from django.shortcuts import get_object_or_404
from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView
from rest_framework.generics import ListAPIView

from users.models import Patient
from .models import DataCollectionDay, SensorReading
from .serializers import (
    DataCollectionDaySerializer,
    SensorReadingSerializer,
    BulkSensorUploadSerializer,
)


def get_patient_for_doctor(doctor, patient_id):
    """
    Helper: retrieve a Patient by ID, ensuring the authenticated doctor
    is in the patient's doctors M2M set. Returns None if not found.
    """
    try:
        return Patient.objects.filter(doctors=doctor, is_active=True).get(pk=patient_id)
    except Patient.DoesNotExist:
        return None


class DataCollectionDayView(APIView):
    """
    POST /api/data/patients/<patient_id>/days/
        Create or retrieve a collection day for a given date and patient.
        Uses get_or_create for idempotency — calling with the same date
        always returns the same day.

        Requires: Authorization: Bearer <access_token>

        Request body:
            {"date": "2026-08-20"}

        Response (201 - created / 200 - already exists):
            {"id": 1, "patient": 3, "date": "2026-08-20", "reading_count": 0, ...}

    GET /api/data/patients/<patient_id>/days/
        List all collection days for a specific patient,
        annotated with reading counts.

        Response (200):
            [{"id": 1, "patient": 3, "date": "2026-08-20", "reading_count": 42, ...}, ...]
    """

    permission_classes = [IsAuthenticated]

    def post(self, request, patient_id):
        patient = get_patient_for_doctor(request.user, patient_id)
        if patient is None:
            return Response(
                {'message': 'Patient not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        serializer = DataCollectionDaySerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        defaults = {}
        if 'device_id' in serializer.validated_data:
            defaults['device_id'] = serializer.validated_data['device_id']
        elif patient.device_id:
            defaults['device_id'] = patient.device_id

        day, created = DataCollectionDay.objects.get_or_create(
            patient=patient,
            date=serializer.validated_data['date'],
            defaults=defaults
        )

        if not created and 'device_id' in serializer.validated_data and day.device_id != serializer.validated_data['device_id']:
            day.device_id = serializer.validated_data['device_id']
            day.save(update_fields=['device_id', 'updated_at'])

        # Re-serialize with reading count annotation
        response_data = DataCollectionDaySerializer(day).data
        response_data['reading_count'] = day.readings.count()

        return Response(
            response_data,
            status=status.HTTP_201_CREATED if created else status.HTTP_200_OK,
        )

    def get(self, request, patient_id):
        patient = get_patient_for_doctor(request.user, patient_id)
        if patient is None:
            return Response(
                {'message': 'Patient not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        days = (
            DataCollectionDay.objects
            .filter(patient=patient)
            .annotate(reading_count=Count('readings'))
            .order_by('-date')
        )
        serializer = DataCollectionDaySerializer(days, many=True)
        return Response(serializer.data)


class DataCollectionDayDetailView(APIView):
    """
    GET /api/data/patients/<patient_id>/days/<id>/

    Returns details of a specific collection day, including summary stats.
    The patient must belong to the authenticated doctor.

    Requires: Authorization: Bearer <access_token>

    Response (200):
        {
            "id": 1,
            "patient": 3,
            "date": "2026-08-20",
            "reading_count": 42,
            "first_reading_time": 1234567890,
            "last_reading_time": 1234567932,
            "created_at": "...",
            "updated_at": "..."
        }

    Response (404):
        {"message": "Collection day not found."}
    """

    permission_classes = [IsAuthenticated]

    def get(self, request, patient_id, pk):
        patient = get_patient_for_doctor(request.user, patient_id)
        if patient is None:
            return Response(
                {'message': 'Patient not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        try:
            day = (
                DataCollectionDay.objects
                .filter(patient=patient, pk=pk)
                .annotate(
                    reading_count=Count('readings'),
                    first_reading_time=Min('readings__time'),
                    last_reading_time=Max('readings__time'),
                )
                .get()
            )
        except DataCollectionDay.DoesNotExist:
            return Response(
                {'message': 'Collection day not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        data = DataCollectionDaySerializer(day).data
        data['first_reading_time'] = day.first_reading_time
        data['last_reading_time'] = day.last_reading_time
        return Response(data)


class SensorUploadView(APIView):
    """
    POST /api/data/patients/<patient_id>/upload/

    Bulk upload sensor readings linked to a DataCollectionDay.
    Uses ignore_conflicts so re-syncing the same data is safe (idempotent).
    The patient must belong to the authenticated doctor.

    Requires: Authorization: Bearer <access_token>

    Request body:
        {
            "collection_day": 1,
            "readings": [
                {"time": 1234567890, "gsr": 512, "hr": 72, "bodyTemp": 36.5, ...},
                {"time": 1234567891, "gsr": 515, "hr": 73, "bodyTemp": 36.6, ...}
            ]
        }

    Response (201):
        {
            "message": "Successfully uploaded 2 readings.",
            "count": 2
        }
    """

    permission_classes = [IsAuthenticated]

    def post(self, request, patient_id):
        patient = get_patient_for_doctor(request.user, patient_id)
        if patient is None:
            return Response(
                {'message': 'Patient not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        serializer = BulkSensorUploadSerializer(
            data=request.data,
            context={'request': request},
        )
        serializer.is_valid(raise_exception=True)

        # Additional check: ensure the collection day belongs to this patient
        collection_day = serializer.validated_data['collection_day']
        if collection_day.patient_id != patient.pk:
            return Response(
                {'message': 'Collection day does not belong to this patient.'},
                status=status.HTTP_400_BAD_REQUEST,
            )

        created = serializer.save()

        return Response(
            {
                'message': f"Successfully uploaded {len(created)} readings.",
                'count': len(created),
            },
            status=status.HTTP_201_CREATED,
        )


class SensorSyncView(ListAPIView):
    """
    GET /api/data/patients/<patient_id>/sync/?after=<unix_timestamp>

    Pull sensor readings after a given timestamp for a specific patient.
    Used by the mobile app to sync data it may have missed.
    Results are paginated (100 per page by default).
    The patient must belong to the authenticated doctor.

    Requires: Authorization: Bearer <access_token>

    Query params:
        after (optional): Unix timestamp. Only readings after this time are returned.

    Response (200):
        {
            "count": 250,
            "next": "http://.../api/data/patients/3/sync/?after=1234567890&page=2",
            "previous": null,
            "results": [ ... ]
        }
    """

    serializer_class = SensorReadingSerializer
    permission_classes = [IsAuthenticated]

    def get_queryset(self):
        patient_id = self.kwargs['patient_id']
        patient = get_patient_for_doctor(self.request.user, patient_id)

        if patient is None:
            return SensorReading.objects.none()

        queryset = SensorReading.objects.filter(
            collection_day__patient=patient,
        )

        after = self.request.query_params.get('after')
        if after is not None:
            try:
                after_ts = int(after)
                queryset = queryset.filter(time__gt=after_ts)
            except (ValueError, TypeError):
                pass  # Ignore invalid 'after' param, return all data

        return queryset.order_by('time')


class SensorLatestView(APIView):
    """
    GET /api/data/patients/<patient_id>/latest/

    Returns the most recent sensor reading for a specific patient.
    Useful for dashboard quick-glance display.
    The patient must belong to the authenticated doctor.

    Requires: Authorization: Bearer <access_token>

    Response (200):
        { "time": 1234567891, "gsr": 515, "hr": 73, ... }

    Response (404):
        { "message": "No sensor readings found." }
    """

    permission_classes = [IsAuthenticated]

    def get(self, request, patient_id):
        patient = get_patient_for_doctor(request.user, patient_id)
        if patient is None:
            return Response(
                {'message': 'Patient not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        latest = (
            SensorReading.objects
            .filter(collection_day__patient=patient)
            .order_by('-time')
            .first()
        )

        if latest is None:
            return Response(
                {'message': 'No sensor readings found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        serializer = SensorReadingSerializer(latest)
        return Response(serializer.data)
