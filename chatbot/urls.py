from django.urls import path

from .views import ChatView, ConversationListView, ConversationDetailView


urlpatterns = [
    path('chat/', ChatView.as_view(), name='chatbot-chat'),
    path('conversations/', ConversationListView.as_view(), name='chatbot-conversations'),
    path('conversations/<int:pk>/', ConversationDetailView.as_view(), name='chatbot-conversation-detail'),
]
