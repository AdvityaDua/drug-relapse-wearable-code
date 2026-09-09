from django.urls import path

from .views import (
    DataCollectionDayView,
    DataCollectionDayDetailView,
    SensorUploadView,
    SensorSyncView,
    SensorLatestView,
)


urlpatterns = [
    # All data routes are nested under a patient
    path('patients/<int:patient_id>/days/', DataCollectionDayView.as_view(), name='collection-days'),
    path('patients/<int:patient_id>/days/<int:pk>/', DataCollectionDayDetailView.as_view(), name='collection-day-detail'),
    path('patients/<int:patient_id>/upload/', SensorUploadView.as_view(), name='data-upload'),
    path('patients/<int:patient_id>/sync/', SensorSyncView.as_view(), name='data-sync'),
    path('patients/<int:patient_id>/latest/', SensorLatestView.as_view(), name='data-latest'),
]
