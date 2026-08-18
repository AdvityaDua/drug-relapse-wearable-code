from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView
from rest_framework.generics import ListAPIView

from .models import SensorReading
from .serializers import SensorReadingSerializer, BulkSensorUploadSerializer


class SensorUploadView(APIView):
    """
    POST /api/data/upload/

    Bulk upload sensor readings from the mobile app.
    Uses ignore_conflicts so re-syncing the same data is safe (idempotent).

    Requires: Authorization: Bearer <access_token>

    Request body:
        {
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
        queryset = SensorReading.objects.filter(user=self.request.user)

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
            .filter(user=request.user)
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
