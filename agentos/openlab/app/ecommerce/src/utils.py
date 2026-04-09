# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
E-commerce Utility Functions
============================

This module provides utility functions for the openlab E-commerce platform.
It includes helper functions for validation, formatting, security, and
commons operations used throughout the e-commerce application.

Features:
- Data validation and sanitization
- Currency and price formatting
- Security utilities (hashing, token generation)
- File handling and upload processing
- Date and time utilities
- Configuration management
"""

import base64
import hashlib
import hmac
import json
import logging
import os
import random
import re
import string
import time
import uuid
from datetime import datetime, timedelta
from decimal import Decimal, ROUND_HALF_UP
from pathlib import Path
from typing import Any, Dict, List, Optional, Tuple, Union
from urllib.parse import urlparse, urljoin

try:
    import bcrypt
    import jwt
    import stripe
    import redis
    from PIL import Image
    from email_validator import validate_email, EmailNotValidError
    BCYPT_AVAILABLE = True
    JWT_AVAILABLE = True
    STRIPE_AVAILABLE = True
    REDIS_AVAILABLE = True
    PILLOW_AVAILABLE = True
    EMAIL_VALIDATOR_AVAILABLE = True
except ImportError:
    BCYPT_AVAILABLE = False
    JWT_AVAILABLE = False
    STRIPE_AVAILABLE = False
    REDIS_AVAILABLE = False
    PILLOW_AVAILABLE = False
    EMAIL_VALIDATOR_AVAILABLE = False


class ValidationError(Exception):
    """Custom exception for validation errors."""
    pass


class SecurityError(Exception):
    """Custom exception for security-related errors."""
    pass


class FileUploadError(Exception):
    """Custom exception for file upload errors."""
    pass


class CurrencyUtils:
    """Utility class for currency operations."""
    
    # ISO 4217 currency codes and symbols
    CURRENCY_SYMBOLS = {
        'USD': '$',
        'EUR': '鈧?,
        'GBP': '拢',
        'JPY': '楼',
        'CAD': 'C$',
        'AUD': 'A$',
        'CHF': 'CHF',
        'CNY': '楼',
        'INR': '鈧?,
        'RUB': '鈧?,
        'BRL': 'R$',
        'MXN': 'Mex$',
        'KRW': '鈧?,
    }
    
    # Currency formatting patterns by locale
    CURRENCY_FORMATS = {
        'en_US': {
            'decimal_separator': '.',
            'thousands_separator': ',',
            'symbol_position': 'before',
            'space_between': False,
        },
        'en_GB': {
            'decimal_separator': '.',
            'thousands_separator': ',',
            'symbol_position': 'before',
            'space_between': False,
        },
        'fr_FR': {
            'decimal_separator': ',',
            'thousands_separator': ' ',
            'symbol_position': 'after',
            'space_between': True,
        },
        'de_DE': {
            'decimal_separator': ',',
            'thousands_separator': '.',
            'symbol_position': 'after',
            'space_between': True,
        },
        'ja_JP': {
            'decimal_separator': '.',
            'thousands_separator': ',',
            'symbol_position': 'before',
            'space_between': False,
        },
    }
    
    @staticmethod
    def format_price(
        amount: Union[Decimal, float, int, str],
        currency: str = 'USD',
        locale: str = 'en_US',
        include_symbol: bool = True,
        decimal_places: int = 2
    ) -> str:
        """
        Format a price amount with currency symbol and proper formatting.
        
        Args:
            amount: The amount to format.
            currency: ISO 4217 currency code.
            locale: Locale for formatting rules.
            include_symbol: Whether to include currency symbol.
            decimal_places: Number of decimal places to show.
            
        Returns:
            Formatted price string.
        """
        # Convert to Decimal for precise arithmetic
        if not isinstance(amount, Decimal):
            amount = Decimal(str(amount))
        
        # Round to specified decimal places
        amount = amount.quantize(
            Decimal(f'0.{"0" * decimal_places}'),
            rounding=ROUND_HALF_UP
        )
        
        # Get formatting rules for locale
        format_rules = CurrencyUtils.CURRENCY_FORMATS.get(
            locale,
            CurrencyUtils.CURRENCY_FORMATS['en_US']
        )
        
        # Format number with separators
        parts = str(amount).split('.')
        integer_part = parts[0]
        decimal_part = parts[1] if len(parts) > 1 else '0' * decimal_places
        
        # Add thousands separator
        if format_rules['thousands_separator']:
            integer_part = re.sub(
                r'(\d)(?=(\d{3})+(?!\d))',
                f'\\1{format_rules["thousands_separator"]}',
                integer_part
            )
        
        # Build formatted number
        formatted_number = (
            f'{integer_part}{format_rules["decimal_separator"]}'
            f'{decimal_part.rjust(decimal_places, "0")}'
        )
        
        # Add currency symbol if requested
        if include_symbol and currency in CurrencyUtils.CURRENCY_SYMBOLS:
            symbol = CurrencyUtils.CURRENCY_SYMBOLS[currency]
            
            if format_rules['symbol_position'] == 'before':
                if format_rules['space_between']:
                    return f'{symbol} {formatted_number}'
                else:
                    return f'{symbol}{formatted_number}'
            else:
                if format_rules['space_between']:
                    return f'{formatted_number} {symbol}'
                else:
                    return f'{formatted_number}{symbol}'
        
        return formatted_number
    
    @staticmethod
    def parse_price(price_str: str, currency: str = 'USD') -> Decimal:
        """
        Parse a price string into a Decimal amount.
        
        Args:
            price_str: Price string to parse.
            currency: ISO 4217 currency code.
            
        Returns:
            Decimal amount.
            
        Raises:
            ValueError: If price string cannot be parsed.
        """
        # Remove currency symbol
        if currency in CurrencyUtils.CURRENCY_SYMBOLS:
            symbol = CurrencyUtils.CURRENCY_SYMBOLS[currency]
            price_str = price_str.replace(symbol, '')
        
        # Remove thousands separators and normalize decimal separator
        price_str = price_str.strip()
        price_str = price_str.replace(',', '').replace(' ', '')
        
        # Try to parse as Decimal
        try:
            return Decimal(price_str)
        except Exception as e:
            raise ValueError(f"Failed to parse price string '{price_str}': {str(e)}")
    
    @staticmethod
    def convert_currency(
        amount: Decimal,
        from_currency: str,
        to_currency: str,
        exchange_rate: Optional[Decimal] = None
    ) -> Decimal:
        """
        Convert amount from one currency to another.
        
        Args:
            amount: Amount to convert.
            from_currency: Source currency code.
            to_currency: Target currency code.
            exchange_rate: Optional exchange rate (from_currency to to_currency).
                          If not provided, uses default rate of 1.0.
            
        Returns:
            Converted amount.
        """
        if exchange_rate is None:
            # In a real application, you would fetch this from an API
            # For now, use a simple mapping
            default_rates = {
                ('USD', 'EUR'): Decimal('0.85'),
                ('USD', 'GBP'): Decimal('0.73'),
                ('USD', 'JPY'): Decimal('110.0'),
                ('EUR', 'USD'): Decimal('1.18'),
                ('GBP', 'USD'): Decimal('1.37'),
                ('JPY', 'USD'): Decimal('0.0091'),
            }
            
            rate_key = (from_currency, to_currency)
            exchange_rate = default_rates.get(rate_key, Decimal('1.0'))
        
        return amount * exchange_rate
    
    @staticmethod
    def calculate_tax(
        amount: Decimal,
        tax_rate: Decimal,
        inclusive: bool = False
    ) -> Tuple[Decimal, Decimal]:
        """
        Calculate tax amount.
        
        Args:
            amount: Base amount.
            tax_rate: Tax rate as decimal (e.g., 0.08 for 8%).
            inclusive: Whether tax is included in the amount.
            
        Returns:
            Tuple of (tax_amount, total_amount).
        """
        if inclusive:
            # Tax is included in the amount
            tax_amount = amount * tax_rate / (Decimal('1') + tax_rate)
            base_amount = amount - tax_amount
            return tax_amount, base_amount
        else:
            # Tax is added to the amount
            tax_amount = amount * tax_rate
            total_amount = amount + tax_amount
            return tax_amount, total_amount
    
    @staticmethod
    def calculate_discount(
        amount: Decimal,
        discount_type: str,
        discount_value: Decimal
    ) -> Decimal:
        """
        Calculate discount amount.
        
        Args:
            amount: Original amount.
            discount_type: 'percentage' or 'fixed'.
            discount_value: Discount value.
            
        Returns:
            Discount amount.
            
        Raises:
            ValueError: If discount_type is invalid.
        """
        if discount_type == 'percentage':
            # Percentage discount
            if discount_value < Decimal('0') or discount_value > Decimal('100'):
                raise ValueError("Percentage discount must be between 0 and 100")
            return amount * discount_value / Decimal('100')
        
        elif discount_type == 'fixed':
            # Fixed amount discount
            if discount_value < Decimal('0'):
                raise ValueError("Fixed discount cannot be negative")
            return min(discount_value, amount)  # Can't discount more than amount
        
        else:
            raise ValueError(f"Invalid discount type: {discount_type}")


class SecurityUtils:
    """Utility class for security operations."""
    
    @staticmethod
    def hash_password(password: str) -> str:
        """
        Hash a password using bcrypt.
        
        Args:
            password: Plain text password.
            
        Returns:
            Hashed password.
            
        Raises:
            ImportError: If bcrypt is not available.
        """
        if not BCYPT_AVAILABLE:
            raise ImportError("bcrypt is required for password hashing")
        
        # Generate salt and hash password
        salt = bcrypt.gensalt()
        hashed = bcrypt.hashpw(password.encode('utf-8'), salt)
        return hashed.decode('utf-8')
    
    @staticmethod
    def verify_password(password: str, hashed_password: str) -> bool:
        """
        Verify a password against a hash.
        
        Args:
            password: Plain text password to verify.
            hashed_password: Hashed password to compare against.
            
        Returns:
            True if password matches, False otherwise.
            
        Raises:
            ImportError: If bcrypt is not available.
        """
        if not BCYPT_AVAILABLE:
            raise ImportError("bcrypt is required for password verification")
        
        try:
            return bcrypt.checkpw(
                password.encode('utf-8'),
                hashed_password.encode('utf-8')
            )
        except Exception:
            return False
    
    @staticmethod
    def generate_jwt_token(
        payload: Dict[str, Any],
        secret_key: str,
        algorithm: str = 'HS256',
        expires_in: Optional[int] = None
    ) -> str:
        """
        Generate a JWT token.
        
        Args:
            payload: Token payload data.
            secret_key: Secret key for signing.
            algorithm: JWT algorithm to use.
            expires_in: Token expiration in seconds.
            
        Returns:
            JWT token string.
            
        Raises:
            ImportError: If PyJWT is not available.
        """
        if not JWT_AVAILABLE:
            raise ImportError("PyJWT is required for JWT token generation")
        
        # Add expiration if specified
        if expires_in:
            payload['exp'] = int(time.time()) + expires_in
        
        # Add issued at time
        payload['iat'] = int(time.time())
        
        # Generate token
        return jwt.encode(payload, secret_key, algorithm=algorithm)
    
    @staticmethod
    def verify_jwt_token(
        token: str,
        secret_key: str,
        algorithms: List[str] = ['HS256']
    ) -> Dict[str, Any]:
        """
        Verify and decode a JWT token.
        
        Args:
            token: JWT token to verify.
            secret_key: Secret key for verification.
            algorithms: List of allowed algorithms.
            
        Returns:
            Decoded token payload.
            
        Raises:
            ImportError: If PyJWT is not available.
            jwt.InvalidTokenError: If token is invalid.
        """
        if not JWT_AVAILABLE:
            raise ImportError("PyJWT is required for JWT token verification")
        
        return jwt.decode(token, secret_key, algorithms=algorithms)
    
    @staticmethod
    def generate_api_key(length: int = 32) -> str:
        """
        Generate a random API key.
        
        Args:
            length: Length of the API key.
            
        Returns:
            Random API key.
        """
        # Use URL-safe base64 encoding
        random_bytes = os.urandom(length)
        return base64.urlsafe_b64encode(random_bytes).decode('utf-8')[:length]
    
    @staticmethod
    def generate_secure_random_string(
        length: int = 16,
        include_digits: bool = True,
        include_special: bool = False
    ) -> str:
        """
        Generate a secure random string.
        
        Args:
            length: Length of the string.
            include_digits: Include digits (0-9).
            include_special: Include special characters.
            
        Returns:
            Random string.
        """
        characters = string.ascii_letters
        
        if include_digits:
            characters += string.digits
        
        if include_special:
            characters += string.punctuation
        
        # Use cryptographically secure random generator
        return ''.join(random.SystemRandom().choice(characters) for _ in range(length))
    
    @staticmethod
    def validate_csrf_token(token: str, secret: str, data: str) -> bool:
        """
        Validate a CSRF token.
        
        Args:
            token: CSRF token to validate.
            secret: Secret key.
            data: Data that was signed.
            
        Returns:
            True if token is valid, False otherwise.
        """
        # Generate expected token
        expected = hmac.new(
            secret.encode('utf-8'),
            data.encode('utf-8'),
            hashlib.sha256
        ).hexdigest()
        
        # Use constant-time comparison to prevent timing attacks
        return hmac.compare_digest(token, expected)
    
    @staticmethod
    def sanitize_input(input_str: str) -> str:
        """
        Sanitize user input to prevent XSS attacks.
        
        Args:
            input_str: Input string to sanitize.
            
        Returns:
            Sanitized string.
        """
        # Remove HTML tags
        sanitized = re.sub(r'<[^>]*>', '', input_str)
        
        # Escape special characters
        sanitized = (
            sanitized
            .replace('&', '&amp;')
            .replace('<', '&lt;')
            .replace('>', '&gt;')
            .replace('"', '&quot;')
            .replace("'", '&#x27;')
            .replace('/', '&#x2F;')
        )
        
        return sanitized
    
    @staticmethod
    def validate_url(url: str, allowed_domains: Optional[List[str]] = None) -> bool:
        """
        Validate a URL.
        
        Args:
            url: URL to validate.
            allowed_domains: List of allowed domains (optional).
            
        Returns:
            True if URL is valid, False otherwise.
        """
        try:
            result = urlparse(url)
            
            # Check scheme
            if result.scheme not in ['http', 'https']:
                return False
            
            # Check netloc (domain)
            if not result.netloc:
                return False
            
            # Check allowed domains if specified
            if allowed_domains:
                domain = result.netloc.lower()
                if not any(domain.endswith(allowed.lower()) for allowed in allowed_domains):
                    return False
            
            return True
        
        except Exception:
            return False


class FileUtils:
    """Utility class for file operations."""
    
    # MIME type to extension mapping
    MIME_TYPES = {
        'image/jpeg': '.jpg',
        'image/jpg': '.jpg',
        'image/png': '.png',
        'image/gif': '.gif',
        'image/webp': '.webp',
        'image/svg+xml': '.svg',
        'application/pdf': '.pdf',
        'application/zip': '.zip',
        'text/plain': '.txt',
        'text/csv': '.csv',
        'application/json': '.json',
    }
    
    @staticmethod
    def validate_file_upload(
        file_content: bytes,
        filename: str,
        max_size: int,
        allowed_extensions: List[str],
        allowed_mime_types: Optional[List[str]] = None
    ) -> Tuple[bool, Optional[str]]:
        """
        Validate a file upload.
        
        Args:
            file_content: File content as bytes.
            filename: Original filename.
            max_size: Maximum file size in bytes.
            allowed_extensions: List of allowed file extensions.
            allowed_mime_types: List of allowed MIME types (optional).
            
        Returns:
            Tuple of (is_valid, error_message).
        """
        # Check file size
        if len(file_content) > max_size:
            return False, f"File size exceeds maximum of {max_size} bytes"
        
        # Check file extension
        file_ext = Path(filename).suffix.lower()
        if file_ext not in allowed_extensions:
            return False, f"File extension {file_ext} not allowed"
        
        # Check MIME type if specified
        if allowed_mime_types and PILLOW_AVAILABLE:
            try:
                # Use PIL to detect image MIME type
                from io import BytesIO
                image = Image.open(BytesIO(file_content))
                mime_type = image.get_format_mimetype()
                
                if mime_type not in allowed_mime_types:
                    return False, f"MIME type {mime_type} not allowed"
            
            except Exception as e:
                return False, f"Failed to validate file type: {str(e)}"
        
        return True, None
    
    @staticmethod
    def generate_unique_filename(
        original_filename: str,
        prefix: str = '',
        suffix: str = ''
    ) -> str:
        """
        Generate a unique filename.
        
        Args:
            original_filename: Original filename.
            prefix: Optional prefix.
            suffix: Optional suffix.
            
        Returns:
            Unique filename.
        """
        # Extract extension
        path = Path(original_filename)
        extension = path.suffix
        
        # Generate unique ID
        unique_id = uuid.uuid4().hex[:8]
        
        # Get base name without extension
        base_name = path.stem
        
        # Build new filename
        new_filename = f"{prefix}{base_name}_{unique_id}{suffix}{extension}"
        
        return new_filename
    
    @staticmethod
    def save_uploaded_file(
        file_content: bytes,
        filename: str,
        upload_dir: str,
        create_thumbnails: bool = False,
        thumbnail_sizes: Optional[List[Tuple[int, int]]] = None
    ) -> Dict[str, str]:
        """
        Save an uploaded file.
        
        Args:
            file_content: File content as bytes.
            filename: Filename to save as.
            upload_dir: Directory to save in.
            create_thumbnails: Whether to create thumbnails (for images).
            thumbnail_sizes: List of (width, height) tuples for thumbnails.
            
        Returns:
            Dictionary with file paths.
            
        Raises:
            FileUploadError: If file cannot be saved.
        """
        try:
            # Create upload directory if it doesn't exist
            upload_path = Path(upload_dir)
            upload_path.mkdir(parents=True, exist_ok=True)
            
            # Save original file
            file_path = upload_path / filename
            with open(file_path, 'wb') as f:
                f.write(file_content)
            
            result = {
                'original': str(file_path),
                'filename': filename,
                'size': len(file_content),
            }
            
            # Create thumbnails if requested and file is an image
            if create_thumbnails and PILLOW_AVAILABLE:
                try:
                    from io import BytesIO
                    image = Image.open(BytesIO(file_content))
                    
                    # Default thumbnail sizes
                    if thumbnail_sizes is None:
                        thumbnail_sizes = [(100, 100), (300, 300), (600, 600)]
                    
                    thumbnails = {}
                    for width, height in thumbnail_sizes:
                        # Create thumbnail
                        thumb = image.copy()
                        thumb.thumbnail((width, height), Image.Resampling.LANCZOS)
                        
                        # Generate thumbnail filename
                        thumb_filename = f"{Path(filename).stem}_{width}x{height}{Path(filename).suffix}"
                        thumb_path = upload_path / thumb_filename
                        
                        # Save thumbnail
                        thumb.save(thumb_path)
                        thumbnails[f"{width}x{height}"] = str(thumb_path)
                    
                    result['thumbnails'] = thumbnails
                
                except Exception as e:
                    # Log thumbnail creation error but don't fail the upload
                    logging.warning(f"Failed to create thumbnails: {str(e)}")
            
            return result
        
        except Exception as e:
            raise FileUploadError(f"Failed to save uploaded file: {str(e)}")
    
    @staticmethod
    def delete_file(filepath: str) -> bool:
        """
        Delete a file.
        
        Args:
            filepath: Path to file to delete.
            
        Returns:
            True if file was deleted, False otherwise.
        """
        try:
            path = Path(filepath)
            if path.exists():
                path.unlink()
                return True
            return False
        except Exception:
            return False
    
    @staticmethod
    def get_file_info(filepath: str) -> Optional[Dict[str, Any]]:
        """
        Get information about a file.
        
        Args:
            filepath: Path to file.
            
        Returns:
            Dictionary with file information, or None if file doesn't exist.
        """
        try:
            path = Path(filepath)
            if not path.exists():
                return None
            
            stat = path.stat()
            
            return {
                'filename': path.name,
                'path': str(path),
                'size': stat.st_size,
                'created': datetime.fromtimestamp(stat.st_ctime),
                'modified': datetime.fromtimestamp(stat.st_mtime),
                'extension': path.suffix,
                'is_file': path.is_file(),
                'is_dir': path.is_dir(),
            }
        
        except Exception:
            return None


class ValidationUtils:
    """Utility class for data validation."""
    
    @staticmethod
    def validate_email(email: str) -> Tuple[bool, Optional[str]]:
        """
        Validate an email address.
        
        Args:
            email: Email address to validate.
            
        Returns:
            Tuple of (is_valid, error_message).
        """
        if not EMAIL_VALIDATOR_AVAILABLE:
            # Simple regex fallback
            email_regex = r'^[a-zA-Z0-9._%+-]+@[a-zA-Z0-9.-]+\.[a-zA-Z]{2,}$'
            if re.match(email_regex, email):
                return True, None
            else:
                return False, "Invalid email format"
        
        try:
            # Use email-validator library
            validate_email(email)
            return True, None
        except EmailNotValidError as e:
            return False, str(e)
    
    @staticmethod
    def validate_phone(phone: str, country_code: str = 'US') -> Tuple[bool, Optional[str]]:
        """
        Validate a phone number.
        
        Args:
            phone: Phone number to validate.
            country_code: ISO country code for validation rules.
            
        Returns:
            Tuple of (is_valid, error_message).
        """
        # Remove all non-digit characters
        digits = re.sub(r'\D', '', phone)
        
        # Basic validation rules by country
        rules = {
            'US': {
                'min_length': 10,
                'max_length': 11,  # Including country code
                'pattern': r'^[2-9]\d{9}$',  # Last 10 digits
            },
            'UK': {
                'min_length': 10,
                'max_length': 11,
                'pattern': r'^(\+44|0)7\d{9}$',
            },
            'CA': {
                'min_length': 10,
                'max_length': 11,
                'pattern': r'^[2-9]\d{9}$',
            },
            'AU': {
                'min_length': 9,
                'max_length': 10,
                'pattern': r'^(\+61|0)4\d{8}$',
            },
        }
        
        rule = rules.get(country_code, rules['US'])
        
        if len(digits) < rule['min_length'] or len(digits) > rule['max_length']:
            return False, f"Phone number must be {rule['min_length']}-{rule['max_length']} digits"
        
        if not re.match(rule['pattern'], digits[-10:] if len(digits) > 10 else digits):
            return False, "Invalid phone number format"
        
        return True, None
    
    @staticmethod
    def validate_password(password: str, manager: Dict[str, Any]) -> Tuple[bool, Optional[str]]:
        """
        Validate a password against security requirements.
        
        Args:
            password: Password to validate.
            manager: Password configuration.
            
        Returns:
            Tuple of (is_valid, error_message).
        """
        min_length = manager.get('min_length', 8)
        require_special = manager.get('require_special', True)
        require_number = manager.get('require_number', True)
        require_uppercase = manager.get('require_uppercase', True)
        
        errors = []
        
        # Check minimum length
        if len(password) < min_length:
            errors.append(f"Password must be at least {min_length} characters long")
        
        # Check for special characters
        if require_special and not re.search(r'[!@#$%^&*(),.?":{}|<>]', password):
            errors.append("Password must contain at least one special character")
        
        # Check for numbers
        if require_number and not re.search(r'\d', password):
            errors.append("Password must contain at least one number")
        
        # Check for uppercase letters
        if require_uppercase and not re.search(r'[A-Z]', password):
            errors.append("Password must contain at least one uppercase letter")
        
        if errors:
            return False, "; ".join(errors)
        
        return True, None
    
    @staticmethod
    def validate_credit_card(
        card_number: str,
        card_type: Optional[str] = None
    ) -> Tuple[bool, Optional[str]]:
        """
        Validate a credit card number using Luhn algorithm.
        
        Args:
            card_number: Credit card number.
            card_type: Optional card type for additional validation.
            
        Returns:
            Tuple of (is_valid, error_message).
        """
        # Remove all non-digit characters
        digits = re.sub(r'\D', '', card_number)
        
        # Check length
        if len(digits) < 13 or len(digits) > 19:
            return False, "Invalid credit card number length"
        
        # Luhn algorithm
        total = 0
        reverse_digits = digits[::-1]
        
        for i, digit in enumerate(reverse_digits):
            n = int(digit)
            if i % 2 == 1:
                n *= 2
                if n > 9:
                    n -= 9
            total += n
        
        if total % 10 != 0:
            return False, "Invalid credit card number"
        
        # Additional validation by card type
        if card_type:
            card_type = card_type.lower()
            
            # Card type patterns (first few digits)
            patterns = {
                'visa': r'^4',
                'mastercard': r'^5[1-5]',
                'amex': r'^3[47]',
                'discover': r'^6(?:011|5)',
                'diners': r'^3(?:0[0-5]|[68])',
                'jcb': r'^35',
            }
            
            if card_type in patterns:
                if not re.match(patterns[card_type], digits):
                    return False, f"Card number does not match {card_type} pattern"
        
        return True, None
    
    @staticmethod
    def validate_postal_code(
        postal_code: str,
        country_code: str = 'US'
    ) -> Tuple[bool, Optional[str]]:
        """
        Validate a postal/zip code.
        
        Args:
            postal_code: Postal code to validate.
            country_code: ISO country code.
            
        Returns:
            Tuple of (is_valid, error_message).
        """
        patterns = {
            'US': r'^\d{5}(-\d{4})?$',
            'CA': r'^[A-Z]\d[A-Z] \d[A-Z]\d$',
            'UK': r'^[A-Z]{1,2}\d[A-Z\d]? ?\d[A-Z]{2}$',
            'AU': r'^\d{4}$',
            'DE': r'^\d{5}$',
            'FR': r'^\d{5}$',
            'JP': r'^\d{3}-\d{4}$',
            'IN': r'^\d{6}$',
            'BR': r'^\d{5}-\d{3}$',
        }
        
        pattern = patterns.get(country_code.upper())
        if not pattern:
            # No validation for this country
            return True, None
        
        if re.match(pattern, postal_code.upper()):
            return True, None
        else:
            return False, f"Invalid postal code format for {country_code}"


class DateUtils:
    """Utility class for date and time operations."""
    
    @staticmethod
    def parse_date(date_str: str, format: str = '%Y-%m-%d') -> Optional[datetime]:
        """
        Parse a date string.
        
        Args:
            date_str: Date string to parse.
            format: Date format string.
            
        Returns:
            datetime object, or None if parsing fails.
        """
        try:
            return datetime.strptime(date_str, format)
        except ValueError:
            return None
    
    @staticmethod
    def format_date(
        date: datetime,
        format: str = '%Y-%m-%d',
        locale: str = 'en_US'
    ) -> str:
        """
        Format a date.
        
        Args:
            date: datetime object to format.
            format: Date format string.
            locale: Locale for formatting.
            
        Returns:
            Formatted date string.
        """
        # In a real application, you might use locale-specific formatting
        # For now, just use strftime
        return date.strftime(format)
    
    @staticmethod
    def humanize_date(date: datetime) -> str:
        """
        Convert a date to human-readable format.
        
        Args:
            date: datetime object.
            
        Returns:
            Human-readable date string.
        """
        now = datetime.now()
        diff = now - date
        
        if diff.days == 0:
            if diff.seconds < 60:
                return "just now"
            elif diff.seconds < 3600:
                minutes = diff.seconds // 60
                return f"{minutes} minute{'s' if minutes != 1 else ''} ago"
            else:
                hours = diff.seconds // 3600
                return f"{hours} hour{'s' if hours != 1 else ''} ago"
        elif diff.days == 1:
            return "yesterday"
        elif diff.days < 7:
            return f"{diff.days} days ago"
        elif diff.days < 30:
            weeks = diff.days // 7
            return f"{weeks} week{'s' if weeks != 1 else ''} ago"
        elif diff.days < 365:
            months = diff.days // 30
            return f"{months} month{'s' if months != 1 else ''} ago"
        else:
            years = diff.days // 365
            return f"{years} year{'s' if years != 1 else ''} ago"
    
    @staticmethod
    def calculate_age(birth_date: datetime, reference_date: Optional[datetime] = None) -> int:
        """
        Calculate age from birth date.
        
        Args:
            birth_date: Date of birth.
            reference_date: Reference date (defaults to today).
            
        Returns:
            Age in years.
        """
        if reference_date is None:
            reference_date = datetime.now()
        
        age = reference_date.year - birth_date.year
        
        # Adjust if birthday hasn't occurred yet this year
        if (reference_date.month, reference_date.day) < (birth_date.month, birth_date.day):
            age -= 1
        
        return age
    
    @staticmethod
    def business_days_between(
        start_date: datetime,
        end_date: datetime,
        holidays: Optional[List[datetime]] = None
    ) -> int:
        """
        Calculate number of business days between two dates.
        
        Args:
            start_date: Start date.
            end_date: End date.
            holidays: List of holiday dates.
            
        Returns:
            Number of business days.
        """
        if holidays is None:
            holidays = []
        
        # Convert to date objects
        start = start_date.date()
        end = end_date.date()
        
        # Ensure start <= end
        if start > end:
            start, end = end, start
        
        # Calculate business days
        business_days = 0
        current = start
        
        while current <= end:
            # Check if it's a weekday (Monday=0, Sunday=6)
            if current.weekday() < 5 and current not in holidays:
                business_days += 1
            current += timedelta(days=1)
        
        return business_days


class ConfigUtils:
    """Utility class for configuration management."""
    
    @staticmethod
    def load_config(config_path: str) -> Dict[str, Any]:
        """
        Load configuration from file.
        
        Args:
            config_path: Path to configuration file.
            
        Returns:
            Configuration dictionary.
            
        Raises:
            FileNotFoundError: If manager file doesn't exist.
            ValueError: If manager file format is invalid.
        """
        path = Path(config_path)
        
        if not path.exists():
            raise FileNotFoundError(f"Configuration file not found: {config_path}")
        
        try:
            with open(path, 'r', encoding='utf-8') as f:
                if path.suffix in ['.yaml', '.yml']:
                    import yaml
                    return yaml.safe_load(f) or {}
                elif path.suffix == '.json':
                    return json.load(f)
                else:
                    # Try to auto-detect
                    content = f.read()
                    try:
                        return json.loads(content)
                    except json.JSONDecodeError:
                        import yaml
                        return yaml.safe_load(content) or {}
        
        except Exception as e:
            raise ValueError(f"Failed to load configuration: {str(e)}")
    
    @staticmethod
    def save_config(manager: Dict[str, Any], config_path: str, format: str = 'yaml') -> None:
        """
        Save configuration to file.
        
        Args:
            manager: Configuration dictionary.
            config_path: Path to save configuration file.
            format: File format ('yaml' or 'json').
            
        Raises:
            ValueError: If format is invalid.
        """
        path = Path(config_path)
        path.parent.mkdir(parents=True, exist_ok=True)
        
        try:
            with open(path, 'w', encoding='utf-8') as f:
                if format.lower() == 'yaml':
                    import yaml
                    yaml.dump(manager, f, default_flow_style=False)
                elif format.lower() == 'json':
                    json.dump(manager, f, indent=2)
                else:
                    raise ValueError(f"Unsupported format: {format}")
        
        except Exception as e:
            raise ValueError(f"Failed to save configuration: {str(e)}")
    
    @staticmethod
    def merge_configs(base: Dict[str, Any], override: Dict[str, Any]) -> Dict[str, Any]:
        """
        Merge two configuration dictionaries.
        
        Args:
            base: Base configuration.
            override: Override configuration.
            
        Returns:
            Merged configuration.
        """
        result = base.copy()
        
        for key, value in override.items():
            if key in result and isinstance(result[key], dict) and isinstance(value, dict):
                # Recursively merge dictionaries
                result[key] = ConfigUtils.merge_configs(result[key], value)
            else:
                # Overwrite or add new value
                result[key] = value
        
        return result
    
    @staticmethod
    def get_config_value(
        manager: Dict[str, Any],
        key_path: str,
        default: Any = None,
        separator: str = '.'
    ) -> Any:
        """
        Get a configuration value using dot notation.
        
        Args:
            manager: Configuration dictionary.
            key_path: Dot-separated key path.
            default: Default value if key not found.
            separator: Key path separator.
            
        Returns:
            Configuration value, or default if not found.
        """
        keys = key_path.split(separator)
        current = manager
        
        for key in keys:
            if isinstance(current, dict) and key in current:
                current = current[key]
            else:
                return default
        
        return current


# Convenience functions
def generate_order_number(prefix: str = 'ORD') -> str:
    """
    Generate a unique order number.
    
    Args:
        prefix: Order number prefix.
        
    Returns:
        Unique order number.
    """
    timestamp = datetime.now().strftime('%Y%m%d%H%M%S')
    random_suffix = random.randint(1000, 9999)
    return f"{prefix}-{timestamp}-{random_suffix}"


def generate_invoice_number(prefix: str = 'INV') -> str:
    """
    Generate a unique invoice number.
    
    Args:
        prefix: Invoice number prefix.
        
    Returns:
        Unique invoice number.
    """
    timestamp = datetime.now().strftime('%Y%m%d')
    sequence = random.randint(100000, 999999)
    return f"{prefix}-{timestamp}-{sequence}"


def calculate_shipping_cost(
    weight: float,
    dimensions: Tuple[float, float, float],
    destination_zip: str,
    shipping_method: str = 'standard'
) -> Decimal:
    """
    Calculate shipping cost.
    
    Args:
        weight: Package weight in kg.
        dimensions: Package dimensions (length, width, height) in cm.
        destination_zip: Destination zip code.
        shipping_method: Shipping method ('standard', 'express', 'overnight').
        
    Returns:
        Shipping cost.
    """
    # Base rates by shipping method
    base_rates = {
        'standard': Decimal('5.99'),
        'express': Decimal('12.99'),
        'overnight': Decimal('24.99'),
    }
    
    # Get base rate
    base_rate = base_rates.get(shipping_method, base_rates['standard'])
    
    # Calculate dimensional weight
    length, width, height = dimensions
    dimensional_weight = (length * width * height) / 5000  # Volumetric divisor
    
    # Use the greater of actual weight and dimensional weight
    chargeable_weight = max(weight, dimensional_weight)
    
    # Add weight-based surcharge
    weight_surcharge = Decimal('0')
    if chargeable_weight > 5:
        weight_surcharge = Decimal(str((chargeable_weight - 5) * 0.5))
    
    # Calculate total
    total = base_rate + weight_surcharge
    
    # Round to 2 decimal places
    return total.quantize(Decimal('0.01'), rounding=ROUND_HALF_UP)


def format_address(
    street: str,
    city: str,
    state: str,
    postal_code: str,
    country: str,
    name: Optional[str] = None,
    company: Optional[str] = None
) -> str:
    """
    Format an address for display.
    
    Args:
        street: Street address.
        city: City.
        state: State/province.
        postal_code: Postal/ZIP code.
        country: Country.
        name: Optional name.
        company: Optional company.
        
    Returns:
        Formatted address string.
    """
    lines = []
    
    if name:
        lines.append(name)
    
    if company:
        lines.append(company)
    
    lines.append(street)
    lines.append(f"{city}, {state} {postal_code}")
    lines.append(country)
    
    return "\n".join(lines)


def mask_credit_card(card_number: str, visible_digits: int = 4) -> str:
    """
    Mask a credit card number for display.
    
    Args:
        card_number: Credit card number.
        visible_digits: Number of digits to keep visible at the end.
        
    Returns:
        Masked credit card number.
    """
    # Remove non-digit characters
    digits = re.sub(r'\D', '', card_number)
    
    if len(digits) <= visible_digits:
        return digits
    
    # Mask all but the last visible_digits
    masked = '鈥? * (len(digits) - visible_digits)
    visible = digits[-visible_digits:]
    
    return f"{masked}{visible}"


def truncate_text(text: str, max_length: int, ellipsis: str = '...') -> str:
    """
    Truncate text to maximum length.
    
    Args:
        text: Text to truncate.
        max_length: Maximum length.
        ellipsis: Ellipsis string to append.
        
    Returns:
        Truncated text.
    """
    if len(text) <= max_length:
        return text
    
    # Try to truncate at word boundary
    truncated = text[:max_length - len(ellipsis)]
    last_space = truncated.rfind(' ')
    
    if last_space > max_length * 0.7:  # Only if we have a reasonable word break
        truncated = truncated[:last_space]
    
    return f"{truncated}{ellipsis}"