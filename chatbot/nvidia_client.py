import os
import logging

from openai import OpenAI

logger = logging.getLogger(__name__)

# NVIDIA NIM API — OpenAI-compatible
NVIDIA_BASE_URL = 'https://integrate.api.nvidia.com/v1'
NVIDIA_MODEL = 'mistralai/mistral-large-2-instruct'

SYSTEM_PROMPT = (
    "You are a compassionate and knowledgeable health & wellness assistant "
    "integrated into a wearable device platform designed to support individuals "
    "in drug relapse prevention and recovery.\n\n"
    "Your role is to:\n"
    "- Help users understand their physiological data from wearable sensors "
    "(heart rate, SpO2, body temperature, galvanic skin response, motion/activity).\n"
    "- Provide empathetic, non-judgmental support and encouragement.\n"
    "- Offer evidence-based wellness tips related to stress management, sleep, "
    "physical activity, and emotional regulation.\n"
    "- Recognize signs of stress or physiological changes that may indicate "
    "elevated risk and gently suggest coping strategies or professional help.\n"
    "- Never provide medical diagnoses or prescribe medications.\n"
    "- Always encourage users to consult healthcare professionals for medical concerns.\n"
    "- Keep responses concise, warm, and actionable.\n"
    "- CRITICAL: If you do not know the answer or lack sufficient information, explicitly state that you do not know. Do not hallucinate, guess, or make up information under any circumstances. Base all responses strictly on provided data or established factual wellness principles.\n\n"
    "If the user asks about something outside health, wellness, or recovery support, "
    "you may respond briefly but gently steer the conversation back to their wellbeing."
)


def get_nvidia_client():
    """Initialize the NVIDIA NIM OpenAI-compatible client."""
    api_key = os.getenv('CHATBOT_API_KEY')
    if not api_key:
        raise ValueError("CHATBOT_API_KEY environment variable is not set.")
    return OpenAI(base_url=NVIDIA_BASE_URL, api_key=api_key)


def get_chat_completion(messages):
    """
    Send a list of messages to the NVIDIA Nemotron model and return
    the assistant's response text.

    Args:
        messages: List of dicts with 'role' and 'content' keys,
                  following the OpenAI chat completion format.

    Returns:
        The assistant's response content as a string.
    """
    client = get_nvidia_client()

    response = client.chat.completions.create(
        model=NVIDIA_MODEL,
        messages=messages,
        temperature=0.6,
        top_p=0.8,
        max_tokens=1024,
    )

    return response.choices[0].message.content
