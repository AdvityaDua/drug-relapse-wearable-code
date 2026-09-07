import logging

from django.db.models import Count
from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView
from rest_framework.generics import ListAPIView

from .models import Conversation, Message
from .serializers import (
    ConversationSerializer,
    ConversationDetailSerializer,
    ChatRequestSerializer,
    MessageSerializer,
)
from .nvidia_client import SYSTEM_PROMPT, get_chat_completion

logger = logging.getLogger(__name__)


class ChatView(APIView):
    """
    POST /api/chatbot/chat/

    Send a message to the AI chatbot. Optionally continue an existing conversation.

    Requires: Authorization: Bearer <access_token>

    Request body (new conversation):
        {"message": "How is my heart rate looking?"}

    Request body (continue conversation):
        {"message": "Tell me more", "conversation_id": 5}

    Response (200):
        {
            "conversation_id": 5,
            "reply": "Based on your recent data...",
            "title": "Heart Rate Analysis"
        }
    """

    permission_classes = [IsAuthenticated]

    def post(self, request):
        serializer = ChatRequestSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)

        user_message = serializer.validated_data['message']
        conversation_id = serializer.validated_data.get('conversation_id')

        # Get or create conversation
        if conversation_id:
            try:
                conversation = Conversation.objects.get(
                    pk=conversation_id,
                    user=request.user,
                )
            except Conversation.DoesNotExist:
                return Response(
                    {'message': 'Conversation not found.'},
                    status=status.HTTP_404_NOT_FOUND,
                )
        else:
            conversation = Conversation.objects.create(user=request.user)

        # Save user message
        Message.objects.create(
            conversation=conversation,
            role='user',
            content=user_message,
        )

        # Build message history for the API call
        history = list(
            conversation.messages
            .order_by('created_at')
            .values('role', 'content')
        )

        # Prepend system prompt
        api_messages = [{'role': 'system', 'content': SYSTEM_PROMPT}] + history

        # Call vLLM API
        try:
            reply = get_chat_completion(api_messages)
        except Exception as e:
            logger.error(f"LLM API error: {e}")
            return Response(
                {'message': 'Chatbot is temporarily unavailable. Please try again later.'},
                status=status.HTTP_503_SERVICE_UNAVAILABLE,
            )

        # Save assistant response
        Message.objects.create(
            conversation=conversation,
            role='assistant',
            content=reply,
        )

        # Auto-generate title from first user message if not set
        if not conversation.title:
            conversation.title = user_message[:100]
            conversation.save(update_fields=['title', 'updated_at'])
        else:
            conversation.save(update_fields=['updated_at'])

        return Response({
            'conversation_id': conversation.id,
            'reply': reply,
            'title': conversation.title,
        })


class ConversationListView(ListAPIView):
    """
    GET /api/chatbot/conversations/

    List all conversations for the authenticated user, ordered by most recent.
    Each conversation includes a message_count.

    Requires: Authorization: Bearer <access_token>

    Response (200):
        [
            {"id": 5, "title": "Heart Rate Analysis", "message_count": 8, ...},
            {"id": 3, "title": "Sleep Quality", "message_count": 4, ...}
        ]
    """

    serializer_class = ConversationSerializer
    permission_classes = [IsAuthenticated]

    def get_queryset(self):
        return (
            Conversation.objects
            .filter(user=self.request.user)
            .annotate(message_count=Count('messages'))
            .order_by('-updated_at')
        )


class ConversationDetailView(APIView):
    """
    GET /api/chatbot/conversations/<id>/

    Retrieve a specific conversation with its full message history.

    Requires: Authorization: Bearer <access_token>

    Response (200):
        {
            "id": 5,
            "title": "Heart Rate Analysis",
            "messages": [
                {"id": 1, "role": "user", "content": "...", "created_at": "..."},
                {"id": 2, "role": "assistant", "content": "...", "created_at": "..."}
            ],
            ...
        }

    Response (404):
        {"message": "Conversation not found."}
    """

    permission_classes = [IsAuthenticated]

    def get(self, request, pk):
        try:
            conversation = Conversation.objects.get(pk=pk, user=request.user)
        except Conversation.DoesNotExist:
            return Response(
                {'message': 'Conversation not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        serializer = ConversationDetailSerializer(conversation)
        return Response(serializer.data)

    def delete(self, request, pk):
        """Delete a conversation and all its messages."""
        try:
            conversation = Conversation.objects.get(pk=pk, user=request.user)
        except Conversation.DoesNotExist:
            return Response(
                {'message': 'Conversation not found.'},
                status=status.HTTP_404_NOT_FOUND,
            )

        conversation.delete()
        return Response(status=status.HTTP_204_NO_CONTENT)
