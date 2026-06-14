"""E-commerce utility functions."""

def format_price(amount, currency="USD"):
    return f"{currency} {amount:.2f}"

def validate_email(email):
    return "@" in email and "." in email.split("@")[-1]
