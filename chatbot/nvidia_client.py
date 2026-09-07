"""
LLM client — OpenAI-compatible wrapper pointed at custom vLLM endpoint.

Reads LLM_BASE_URL, LLM_MODEL_NAME, LLM_API_KEY from .env.
Replaces the previous NVIDIA NIM integration with a self-hosted
vLLM model deployed behind a /llm proxy.
"""

import os
import logging

from dotenv import load_dotenv
from openai import OpenAI

# Load .env from project root
load_dotenv()

logger = logging.getLogger(__name__)

# ── vLLM endpoint config (read from .env, with sensible defaults) ──
VLLM_BASE_URL = os.getenv('LLM_BASE_URL', 'https://8080-66976dwa2.brevlab.com/llm/v1')
VLLM_MODEL = os.getenv('LLM_MODEL_NAME', 'llama70b')
VLLM_API_KEY = os.getenv('LLM_API_KEY', 'not-needed')

SYSTEM_PROMPT = (
    "You are a compassionate and knowledgeable health & wellness assistant "
    "integrated into a wearable device platform designed to support individuals "
    "in drug relapse prevention and recovery.\n\n"
    "IMPORTANT — DATA STATUS: No wearable sensor data has been synced yet for this user. "
    "You do NOT have access to any live or historical readings (heart rate, SpO2, "
    "body temperature, GSR, motion/activity). If the user asks about their data, "
    "readings, or vitals, you MUST clearly tell them that no data is available yet "
    "and suggest they sync their wearable device. Do NOT make up, estimate, or "
    "hallucinate any numbers or sensor values.\n\n"
    "Your role is to:\n"
    "- Help users understand their physiological data from wearable sensors "
    "(heart rate, SpO2, body temperature, galvanic skin response, motion/activity) "
    "once data becomes available.\n"
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


def get_llm_client():
    """Initialize the OpenAI-compatible client pointed at the vLLM endpoint."""
    return OpenAI(base_url=VLLM_BASE_URL, api_key=VLLM_API_KEY)


def get_chat_completion(messages):
    """
    Send a list of messages to the vLLM-hosted model and return
    the assistant's response text.

    Args:
        messages: List of dicts with 'role' and 'content' keys,
                  following the OpenAI chat completion format.

    Returns:
        The assistant's response content as a string.
    """
    client = get_llm_client()

    response = client.chat.completions.create(
        model=VLLM_MODEL,
        messages=messages,
        temperature=0.6,
        top_p=0.8,
        max_tokens=1024,
    )

    return response.choices[0].message.content
