from django.db.models import Count, Min, Max
from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView
from rest_framework.generics import ListAPIView

from .models import DataCollectionDay, SensorReading
from .serializers import (
    DataCollectionDaySerializer,
    SensorReadingSerializer,
    BulkSensorUploadSerializer,
)


class DataCollectionDayView(APIView):
    """
    POST /api/data/days/
        Create or retrieve a collection day for a given date.
        Uses get_or_create for idempotency — calling with the same date
        always returns the same day.

        Requires: Authorization: Bearer <access_token>

        Request body:
            {"date": "2026-08-20"}

        Response (201 - created / 200 - already exists):
            {"id": 1, "date": "2026-08-20", "reading_count": 0, ...}

    GET /api/data/days/
        List all collection days for the authenticated user,
        annotated with reading counts.

        Response (200):
            [{"id": 1, "date": "2026-08-20", "reading_count": 42, ...}, ...]
    """

    permission_classes = [IsAuthenticated]

    def post(self, request):
        serializer = DataCollectionDaySerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        day, created = DataCollectionDay.objects.get_or_create(
            user=request.user,
            date=serializer.validated_data['date'],
        )

        # Re-serialize with reading count annotation
        response_data = DataCollectionDaySerializer(day).data
        response_data['reading_count'] = day.readings.count()

        return Response(
            response_data,
            status=status.HTTP_201_CREATED if created else status.HTTP_200_OK,
        )

    def get(self, request):
        days = (
            DataCollectionDay.objects
            .filter(user=request.user)
            .annotate(reading_count=Count('readings'))
            .order_by('-date')
        )
        serializer = DataCollectionDaySerializer(days, many=True)
        return Response(serializer.data)


class DataCollectionDayDetailView(APIView):
    """
    GET /api/data/days/<id>/

    Returns details of a specific collection day, including summary stats.

    Requires: Authorization: Bearer <access_token>

    Response (200):
        {
            "id": 1,
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

    def get(self, request, pk):
        try:
            day = (
                DataCollectionDay.objects
                .filter(user=request.user, pk=pk)
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
    POST /api/data/upload/

    Bulk upload sensor readings linked to a DataCollectionDay.
    Uses ignore_conflicts so re-syncing the same data is safe (idempotent).

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

    def post(self, request):
        serializer = BulkSensorUploadSerializer(
            data=request.data,
            context={'request': request},
        )
        serializer.is_valid(raise_exception=True)
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
    GET /api/data/sync/?after=<unix_timestamp>

    Pull sensor readings after a given timestamp for the authenticated user.
    Used by the mobile app to sync data it may have missed.
    Results are paginated (100 per page by default).

    Requires: Authorization: Bearer <access_token>

    Query params:
        after (optional): Unix timestamp. Only readings after this time are returned.

    Response (200):
        {
            "count": 250,
            "next": "http://.../api/data/sync/?after=1234567890&page=2",
            "previous": null,
            "results": [ ... ]
        }
    """

    serializer_class = SensorReadingSerializer
    permission_classes = [IsAuthenticated]

    def get_queryset(self):
        queryset = SensorReading.objects.filter(
            collection_day__user=self.request.user,
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
    GET /api/data/latest/

    Returns the most recent sensor reading for the authenticated user.
    Useful for dashboard quick-glance display.

    Requires: Authorization: Bearer <access_token>

    Response (200):
        { "time": 1234567891, "gsr": 515, "hr": 73, ... }

    Response (404):
        { "message": "No sensor readings found." }
    """

    permission_classes = [IsAuthenticated]

    def get(self, request):
        latest = (
            SensorReading.objects
            .filter(collection_day__user=request.user)
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
