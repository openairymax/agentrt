# Copyright (c) 2026 SPHARX. All Rights Reserved.
# "From data intelligence emerges."

"""
E-commerce Platform Main Application
====================================

This module provides the main FastAPI application for the openlab E-commerce
platform. It implements RESTful API endpoints for product management, order
processing, payment processing, user authentication, inventory management,
shipping integration, and analytics reporting.

Architecture:
- FastAPI for high-performance async API
- SQLAlchemy for database ORM
- Redis for caching and session management
- Stripe for payment processing
- JWT for authentication

Features:
- Complete product CRUD operations
- Shopping cart management
- Order processing workflow
- Stripe payment integration
- User authentication and authorization
- Inventory tracking
- Shipping rate calculation
- Analytics and reporting
"""

import asyncio
import json
import logging
import os
import sys
import traceback
from contextlib import asynccontextmanager
from datetime import datetime, timedelta
from decimal import Decimal
from typing import Any, Dict, List, Optional

import yaml

try:
    from fastapi import FastAPI, Request, HTTPException, Depends, status, BackgroundTasks
    from fastapi.middleware.cors import CORSMiddleware
    from fastapi.responses import JSONResponse, HTMLResponse, FileResponse
    from fastapi.staticfiles import StaticFiles
    from fastapi.security import HTTPBearer, HTTPAuthorizationCredentials
    from fastapi.templating import Jinja2Templates
    from pydantic import BaseModel, Field, field_validator, EmailStr
    import uvicorn
    FASTAPI_AVAILABLE = True
except ImportError:
    FASTAPI_AVAILABLE = False

try:
    from sqlalchemy import create_engine, Column, Integer, String, Float, Boolean, DateTime, ForeignKey, Text, JSON, Enum as SQLEnum
    from sqlalchemy.ext.declarative import declarative_base
    from sqlalchemy.ext.asyncio import AsyncSession, create_async_engine
    from sqlalchemy.orm import sessionmaker, relationship, Session
    SQLALCHEMY_AVAILABLE = True
except ImportError:
    SQLALCHEMY_AVAILABLE = False

try:
    import redis.asyncio as redis
    REDIS_AVAILABLE = True
except ImportError:
    REDIS_AVAILABLE = False

try:
    import stripe
    STRIPE_AVAILABLE = True
except ImportError:
    STRIPE_AVAILABLE = False

try:
    import jwt
    from passlib.context import CryptContext
    JWT_AVAILABLE = True
except ImportError:
    JWT_AVAILABLE = False


logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger(__name__)


if not FASTAPI_AVAILABLE:
    logger.warning("FastAPI not available. Install with: pip install fastapi uvicorn")
if not SQLALCHEMY_AVAILABLE:
    logger.warning("SQLAlchemy not available. Install with: pip install sqlalchemy")
if not REDIS_AVAILABLE:
    logger.warning("Redis not available. Install with: pip install redis")
if not STRIPE_AVAILABLE:
    logger.warning("Stripe not available. Install with: pip install stripe")
if not JWT_AVAILABLE:
    logger.warning("JWT/Passlib not available. Install with: pip install python-jose passlib")


Base = declarative_base()
security = HTTPBearer()
pwd_context = CryptContext(schemes=["bcrypt"], deprecated="auto")


class Product(Base):
    """Product database model."""
    __tablename__ = "products"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(255), nullable=False, index=True)
    description = Column(Text)
    price = Column(Float, nullable=False)
    currency = Column(String(3), default="USD")
    category = Column(String(100), index=True)
    subcategory = Column(String(100))
    sku = Column(String(100), unique=True, index=True)
    stock_quantity = Column(Integer, default=0)
    low_stock_threshold = Column(Integer, default=10)
    is_active = Column(Boolean, default=True)
    is_featured = Column(Boolean, default=False)
    images = Column(JSON, default=list)
    attributes = Column(JSON, default=dict)
    tags = Column(JSON, default=list)
    weight = Column(Float)
    dimensions = Column(JSON)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class User(Base):
    """User database model."""
    __tablename__ = "users"

    id = Column(Integer, primary_key=True, index=True)
    email = Column(String(255), unique=True, nullable=False, index=True)
    username = Column(String(100), unique=True, nullable=False, index=True)
    hashed_password = Column(String(255), nullable=False)
    first_name = Column(String(100))
    last_name = Column(String(100))
    phone = Column(String(20))
    is_active = Column(Boolean, default=True)
    is_verified = Column(Boolean, default=False)
    is_admin = Column(Boolean, default=False)
    addresses = Column(JSON, default=list)
    preferences = Column(JSON, default=dict)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class Order(Base):
    """Order database model."""
    __tablename__ = "orders"

    id = Column(Integer, primary_key=True, index=True)
    order_number = Column(String(50), unique=True, nullable=False, index=True)
    user_id = Column(Integer, ForeignKey("users.id"), nullable=False)
    status = Column(String(50), default="pending", index=True)
    items = Column(JSON, default=list)
    subtotal = Column(Float, nullable=False)
    tax = Column(Float, default=0.0)
    shipping_cost = Column(Float, default=0.0)
    discount = Column(Float, default=0.0)
    total = Column(Float, nullable=False)
    currency = Column(String(3), default="USD")
    shipping_address = Column(JSON)
    billing_address = Column(JSON)
    payment_method = Column(String(50))
    payment_intent_id = Column(String(255))
    payment_status = Column(String(50), default="pending")
    notes = Column(Text)
    tracking_number = Column(String(255))
    shipping carrier = Column(String(100))
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class Cart(Base):
    """Shopping cart database model."""
    __tablename__ = "carts"

    id = Column(Integer, primary_key=True, index=True)
    user_id = Column(Integer, ForeignKey("users.id"), nullable=False)
    session_id = Column(String(255), index=True)
    items = Column(JSON, default=list)
    coupon_code = Column(String(50))
    discount_amount = Column(Float, default=0.0)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class Category(Base):
    """Product category database model."""
    __tablename__ = "categories"

    id = Column(Integer, primary_key=True, index=True)
    name = Column(String(100), nullable=False, unique=True)
    slug = Column(String(100), unique=True, index=True)
    description = Column(Text)
    parent_id = Column(Integer, ForeignKey("categories.id"))
    image_url = Column(String(500))
    is_active = Column(Boolean, default=True)
    sort_order = Column(Integer, default=0)
    created_at = Column(DateTime, default=datetime.utcnow)


class PaymentTransaction(Base):
    """Payment transaction database model."""
    __tablename__ = "payment_transactions"

    id = Column(Integer, primary_key=True, index=True)
    order_id = Column(Integer, ForeignKey("orders.id"), nullable=False)
    stripe_payment_intent_id = Column(String(255), unique=True)
    amount = Column(Float, nullable=False)
    currency = Column(String(3), default="USD")
    status = Column(String(50), nullable=False)
    payment_method = Column(String(50))
    metadata = Column(JSON)
    created_at = Column(DateTime, default=datetime.utcnow)
    updated_at = Column(DateTime, default=datetime.utcnow, onupdate=datetime.utcnow)


class Review(Base):
    """Product review database model."""
    __tablename__ = "reviews"

    id = Column(Integer, primary_key=True, index=True)
    product_id = Column(Integer, ForeignKey("products.id"), nullable=False)
    user_id = Column(Integer, ForeignKey("users.id"), nullable=False)
    rating = Column(Integer, nullable=False)
    title = Column(String(255))
    content = Column(Text)
    is_verified_purchase = Column(Boolean, default=False)
    is_approved = Column(Boolean, default=True)
    helpful_count = Column(Integer, default=0)
    created_at = Column(DateTime, default=datetime.utcnow)


class ShippingRate(Base):
    """Shipping rate database model."""
    __tablename__ = "shipping_rates"

    id = Column(Integer, primary_key=True, index=True)
    carrier = Column(String(100), nullable=False)
    service = Column(String(100), nullable=False)
    zone = Column(String(50))
    min_weight = Column(Float, default=0.0)
    max_weight = Column(Float)
    base_rate = Column(Float, nullable=False)
    per_unit_rate = Column(Float, default=0.0)
    currency = Column(String(3), default="USD")
    estimated_days_min = Column(Integer)
    estimated_days_max = Column(Integer)
    is_active = Column(Boolean, default=True)


class Coupon(Base):
    """Discount coupon database model."""
    __tablename__ = "coupons"

    id = Column(Integer, primary_key=True, index=True)
    code = Column(String(50), unique=True, nullable=False, index=True)
    description = Column(Text)
    discount_type = Column(String(20), nullable=False)
    discount_value = Column(Float, nullable=False)
    min_order_amount = Column(Float, default=0.0)
    max_discount_amount = Column(Float)
    usage_limit = Column(Integer)
    used_count = Column(Integer, default=0)
    valid_from = Column(DateTime, nullable=False)
    valid_until = Column(DateTime, nullable=False)
    is_active = Column(Boolean, default=True)
    applicable_products = Column(JSON, default=list)
    applicable_categories = Column(JSON, default=list)


class PydanticProduct(BaseModel):
    """Pydantic model for product data validation."""
    id: Optional[int] = None
    name: str = Field(..., min_length=1, max_length=255)
    description: Optional[str] = None
    price: float = Field(..., gt=0)
    currency: str = Field(default="USD", max_length=3)
    category: Optional[str] = None
    subcategory: Optional[str] = None
    sku: Optional[str] = Field(None, max_length=100)
    stock_quantity: int = Field(default=0, ge=0)
    low_stock_threshold: int = Field(default=10, ge=0)
    is_active: bool = True
    is_featured: bool = False
    images: List[str] = []
    attributes: Dict[str, Any] = {}
    tags: List[str] = []

    class manager:
        from_attributes = True


class PydanticUser(BaseModel):
    """Pydantic model for user data validation."""
    id: Optional[int] = None
    email: EmailStr
    username: str = Field(..., min_length=3, max_length=100)
    password: Optional[str] = Field(None, min_length=8)
    first_name: Optional[str] = Field(None, max_length=100)
    last_name: Optional[str] = Field(None, max_length=100)
    phone: Optional[str] = Field(None, max_length=20)
    is_active: bool = True
    is_verified: bool = False
    is_admin: bool = False

    class manager:
        from_attributes = True


class PydanticOrder(BaseModel):
    """Pydantic model for order data validation."""
    id: Optional[int] = None
    order_number: Optional[str] = None
    user_id: int
    status: str = "pending"
    items: List[Dict[str, Any]] = []
    subtotal: float = Field(..., ge=0)
    tax: float = Field(default=0.0, ge=0)
    shipping_cost: float = Field(default=0.0, ge=0)
    discount: float = Field(default=0.0, ge=0)
    total: float = Field(..., ge=0)
    currency: str = Field(default="USD", max_length=3)
    shipping_address: Optional[Dict[str, Any]] = None
    billing_address: Optional[Dict[str, Any]] = None
    payment_method: Optional[str] = None

    class manager:
        from_attributes = True


class PydanticCartItem(BaseModel):
    """Pydantic model for cart item data validation."""
    product_id: int
    quantity: int = Field(..., ge=1, le=100)
    price: float = Field(..., gt=0)


class PydanticAddress(BaseModel):
    """Pydantic model for address data validation."""
    street: str
    city: str
    state: str
    postal_code: str
    country: str = Field(default="US", max_length=2)
    is_default: bool = False


class manager:
    """Application configuration manager."""
    _instance = None
    _config = None

    def __new__(cls):
        if cls._instance is None:
            cls._instance = super().__new__(cls)
        return cls._instance

    def load(self, config_path: str = "manager.yaml") -> Dict[str, Any]:
        """Load configuration from YAML file."""
        if self._config is None:
            try:
                with open(config_path, 'r') as f:
                    self._config = yaml.safe_load(f)
            except FileNotFoundError:
                logger.warning(f"manager file {config_path} not found, using defaults")
                self._config = self._default_config()
        return self._config

    def _default_config(self) -> Dict[str, Any]:
        """Return default configuration."""
        return {
            "database_url": "sqlite:///./ecommerce.db",
            "redis_url": "redis://localhost:6379",
            "stripe_secret_key": os.getenv("STRIPE_SECRET_KEY", ""),
            "stripe_public_key": os.getenv("STRIPE_PUBLIC_KEY", ""),
            "jwt_secret_key": os.getenv("JWT_SECRET_KEY", secrets.token_urlsafe(32)),
            "jwt_algorithm": "HS256",
            "jwt_access_token_expire_minutes": 30,
            "cors_origins": ["http://localhost:3000", "http://localhost:8000"],
            "debug": False,
            "host": "0.0.0.0",
            "port": 8000,
            "security": {
                "bcrypt_rounds": 12,
                "rate_limit_requests": 100,
                "rate_limit_period": 60
            },
            "products": {
                "default_currency": "USD",
                "default_tax_rate": 0.08,
                "default_shipping_cost": 5.99,
                "free_shipping_threshold": 50.00
            },
            "uploads": {
                "max_file_size": 10485760,
                "allowed_extensions": [".jpg", ".jpeg", ".png", ".gif", ".webp"],
                "upload_dir": "./uploads"
            }
        }

    def get(self, key: str, default: Any = None) -> Any:
        """Get configuration value by key."""
        if self._config is None:
            self.load()
        keys = key.split(".")
        value = self._config
        for k in keys:
            if isinstance(value, dict):
                value = value.get(k)
            else:
                return default
        return value if value is not None else default


class DatabaseManager:
    """Database connection and session manager."""

    def __init__(self, database_url: str):
        self.database_url = database_url
        self.engine = None
        self.async_engine = None
        self.session_factory = None
        self._initialize()

    def _initialize(self):
        """Initialize database engine and session factory."""
        if SQLALCHEMY_AVAILABLE:
            try:
                if self.database_url.startswith("sqlite"):
                    self.engine = create_engine(
                        self.database_url,
                        connect_args={"check_same_thread": False} if "sqlite" in self.database_url else {},
                        echo=False
                    )
                else:
                    self.engine = create_engine(self.database_url, pool_pre_ping=True)

                Base.metadata.create_all(bind=self.engine)

                self.session_factory = sessionmaker(bind=self.engine, autocommit=False, autoflush=False)

                logger.info(f"Database initialized: {self.database_url}")
            except Exception as e:
                logger.error(f"Database initialization failed: {e}")
                raise

    def get_session(self) -> Session:
        """Get a synchronous database session."""
        if self.session_factory is None:
            raise RuntimeError("Database not initialized")
        return self.session_factory()

    @asynccontextmanager
    async def get_async_session(self):
        """Get an asynchronous database session."""
        if not SQLALCHEMY_AVAILABLE:
            yield None
        else:
            session = self.get_session()
            try:
                yield session
                session.commit()
            except Exception:
                session.rollback()
                raise
            finally:
                session.close()


class CacheManager:
    """Redis cache manager for the e-commerce platform."""

    def __init__(self, redis_url: str, default_ttl: int = 3600):
        self.redis_url = redis_url
        self.default_ttl = default_ttl
        self.client = None
        self._connected = False
        self._connect()

    def _connect(self):
        """Establish connection to Redis."""
        if REDIS_AVAILABLE:
            try:
                self.client = redis.from_url(
                    self.redis_url,
                    encoding="utf-8",
                    decode_responses=True
                )
                self._connected = True
                logger.info(f"Redis connected: {self.redis_url}")
            except Exception as e:
                logger.warning(f"Redis connection failed: {e}")
                self._connected = False

    async def get(self, key: str) -> Optional[str]:
        """Get value from cache."""
        if not self._connected or self.client is None:
            return None
        try:
            return await self.client.get(key)
        except Exception as e:
            logger.error(f"Cache get error: {e}")
            return None

    async def set(self, key: str, value: str, ttl: Optional[int] = None) -> bool:
        """Set value in cache with optional TTL."""
        if not self._connected or self.client is None:
            return False
        try:
            await self.client.set(key, value, ex=ttl or self.default_ttl)
            return True
        except Exception as e:
            logger.error(f"Cache set error: {e}")
            return False

    async def delete(self, key: str) -> bool:
        """Delete key from cache."""
        if not self._connected or self.client is None:
            return False
        try:
            await self.client.delete(key)
            return True
        except Exception as e:
            logger.error(f"Cache delete error: {e}")
            return False

    async def invalidate_pattern(self, pattern: str) -> int:
        """Invalidate all keys matching pattern."""
        if not self._connected or self.client is None:
            return 0
        try:
            keys = await self.client.keys(pattern)
            if keys:
                return await self.client.delete(*keys)
            return 0
        except Exception as e:
            logger.error(f"Cache invalidate error: {e}")
            return 0


class PaymentService:
    """Stripe payment processing service."""

    def __init__(self, secret_key: str):
        self.secret_key = secret_key
        self._client = None
        if STRIPE_AVAILABLE and secret_key:
            try:
                stripe.api_key = secret_key
                self._client = stripe
                logger.info("Stripe payment service initialized")
            except Exception as e:
                logger.error(f"Stripe initialization failed: {e}")

    def create_payment_intent(
        self,
        amount: float,
        currency: str = "usd",
        metadata: Optional[Dict[str, Any]] = None
    ) -> Dict[str, Any]:
        """Create a Stripe payment intent."""
        if not self._client:
            return {"error": "Stripe not configured", "id": None}

        try:
            intent = self._client.PaymentIntent.create(
                amount=int(amount * 100),
                currency=currency,
                metadata=metadata or {},
                automatic_payment_methods={"enabled": True}
            )
            return {"id": intent.id, "client_secret": intent.client_secret, "status": intent.status}
        except Exception as e:
            logger.error(f"Payment intent creation failed: {e}")
            return {"error": str(e), "id": None}

    def retrieve_payment_intent(self, payment_intent_id: str) -> Dict[str, Any]:
        """Retrieve payment intent status."""
        if not self._client:
            return {"error": "Stripe not configured"}

        try:
            intent = self._client.PaymentIntent.retrieve(payment_intent_id)
            return {
                "id": intent.id,
                "amount": intent.amount / 100,
                "currency": intent.currency,
                "status": intent.status,
                "metadata": dict(intent.metadata)
            }
        except Exception as e:
            logger.error(f"Payment intent retrieval failed: {e}")
            return {"error": str(e)}

    def create_refund(self, payment_intent_id: str, amount: Optional[float] = None) -> Dict[str, Any]:
        """Create a refund for a payment."""
        if not self._client:
            return {"error": "Stripe not configured"}

        try:
            refund_params = {"payment_intent": payment_intent_id}
            if amount:
                refund_params["amount"] = int(amount * 100)

            refund = self._client.Refund.create(**refund_params)
            return {"id": refund.id, "amount": refund.amount / 100, "status": refund.status}
        except Exception as e:
            logger.error(f"Refund creation failed: {e}")
            return {"error": str(e)}


class AuthService:
    """JWT-based authentication service."""

    def __init__(self, secret_key: str, algorithm: str = "HS256", expire_minutes: int = 30):
        self.secret_key = secret_key
        self.algorithm = algorithm
        self.expire_minutes = expire_minutes
        self._available = JWT_AVAILABLE

    def hash_password(self, password: str) -> str:
        """Hash a password using bcrypt."""
        if not self._available:
            import hashlib
            return hashlib.sha256(password.encode()).hexdigest()
        return pwd_context.hash(password)

    def verify_password(self, plain_password: str, hashed_password: str) -> bool:
        """Verify a password against its hash."""
        if not self._available:
            import hashlib
            return hashlib.sha256(plain_password.encode()).hexdigest() == hashed_password
        return pwd_context.verify(plain_password, hashed_password)

    def create_access_token(self, data: Dict[str, Any], expires_delta: Optional[timedelta] = None) -> str:
        """Create a JWT access token."""
        to_encode = data.copy()
        expire = datetime.utcnow() + (expires_delta or timedelta(minutes=self.expire_minutes))
        to_encode.update({"exp": expire, "iat": datetime.utcnow()})
        return jwt.encode(to_encode, self.secret_key, algorithm=self.algorithm)

    def decode_token(self, token: str) -> Optional[Dict[str, Any]]:
        """Decode and validate a JWT token."""
        if not self._available:
            return None
        try:
            return jwt.decode(token, self.secret_key, algorithms=[self.algorithm])
        except jwt.ExpiredSignatureError:
            logger.warning("Token expired")
            return None
        except jwt.InvalidTokenError as e:
            logger.warning(f"Invalid token: {e}")
            return None


class EcommerceApp:
    """Main e-commerce application class."""

    def __init__(self, config_path: str = "manager.yaml"):
        """Initialize the e-commerce application."""
        self.manager = manager()
        self.manager.load(config_path)

        db_url = self.manager.get("database_url", "sqlite:///./ecommerce.db")
        redis_url = self.manager.get("redis_url", "redis://localhost:6379")
        stripe_key = self.manager.get("stripe_secret_key", "")
        jwt_secret = self.manager.get("jwt_secret_key", None)
        if not jwt_secret:
            import secrets
            jwt_secret = secrets.token_urlsafe(32)
            import logging
            logging.warning(
                "[SECURITY] jwt_secret_key not configured. "
                "Generated random secret for this session only. "
                "Set jwt_secret_key in config for production!"
            )
        jwt_algorithm = self.manager.get("jwt_algorithm", "HS256")
        jwt_expire = self.manager.get("jwt_access_token_expire_minutes", 30)

        self.db = DatabaseManager(db_url)
        self.cache = CacheManager(redis_url)
        self.payment = PaymentService(stripe_key)
        self.auth = AuthService(jwt_secret, jwt_algorithm, jwt_expire)

        self.app = None
        self._setup_fastapi()

    def _setup_fastapi(self):
        """Setup FastAPI application with routes."""
        if not FASTAPI_AVAILABLE:
            logger.error("FastAPI not available")
            return

        @asynccontextmanager
        async def lifespan(app: FastAPI):
            """Application lifespan handler."""
            logger.info("E-commerce application starting up")
            yield
            logger.info("E-commerce application shutting down")

        self.app = FastAPI(
            title="openlab E-commerce Platform",
            description="A comprehensive e-commerce solution for the openlab marketplace",
            version="1.0.0",
            lifespan=lifespan
        )

        cors_origins = self.manager.get("cors_origins", ["http://localhost:3000"])
        self.app.add_middleware(
            CORSMiddleware,
            allow_origins=cors_origins,
            allow_credentials=True,
            allow_methods=["*"],
            allow_headers=["*"],
        )

        self._setup_routes()

    def _setup_routes(self):
        """Setup API routes."""
        if self.app is None:
            return

        @self.app.get("/")
        async def root():
            """Root endpoint."""
            return {
                "service": "openlab E-commerce Platform",
                "version": "1.0.0",
                "status": "operational"
            }

        @self.app.get("/health")
        async def health_check():
            """Health check endpoint."""
            return {"status": "healthy", "timestamp": datetime.utcnow().isoformat()}

        @self.app.post("/api/auth/register", status_code=status.HTTP_201_CREATED)
        async def register_user(user: PydanticUser):
            """Register a new user."""
            session = self.db.get_session()
            try:
                existing = session.query(User).filter(
                    (User.email == user.email) | (User.username == user.username)
                ).first()
                if existing:
                    raise HTTPException(status_code=400, detail="User already exists")

                db_user = User(
                    email=user.email,
                    username=user.username,
                    hashed_password=self.auth.hash_password(user.password or "default"),
                    first_name=user.first_name,
                    last_name=user.last_name,
                    phone=user.phone
                )
                session.add(db_user)
                session.commit()
                session.refresh(db_user)

                token = self.auth.create_access_token(data={"sub": str(db_user.id), "email": db_user.email})

                return {
                    "id": db_user.id,
                    "email": db_user.email,
                    "username": db_user.username,
                    "access_token": token,
                    "token_type": "bearer"
                }
            except Exception as e:
                session.rollback()
                logger.error(f"Registration failed: {e}")
                raise HTTPException(status_code=500, detail=str(e))
            finally:
                session.close()

        @self.app.post("/api/auth/login")
        async def login(email: str, password: str):
            """Authenticate user and return JWT token."""
            session = self.db.get_session()
            try:
                user = session.query(User).filter(User.email == email).first()
                if not user or not self.auth.verify_password(password, user.hashed_password):
                    raise HTTPException(status_code=401, detail="Invalid credentials")

                if not user.is_active:
                    raise HTTPException(status_code=403, detail="Account is inactive")

                token = self.auth.create_access_token(
                    data={"sub": str(user.id), "email": user.email, "is_admin": user.is_admin}
                )

                return {
                    "access_token": token,
                    "token_type": "bearer",
                    "user": {
                        "id": user.id,
                        "email": user.email,
                        "username": user.username,
                        "is_admin": user.is_admin
                    }
                }
            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Login failed: {e}")
                raise HTTPException(status_code=500, detail="Login failed")
            finally:
                session.close()

        @self.app.get("/api/products")
        async def list_products(
            category: Optional[str] = None,
            featured: Optional[bool] = None,
            search: Optional[str] = None,
            page: int = 1,
            limit: int = 20
        ):
            """List products with filtering and pagination."""
            session = self.db.get_session()
            try:
                query = session.query(Product).filter(Product.is_active == True)

                if category:
                    query = query.filter(Product.category == category)
                if featured is not None:
                    query = query.filter(Product.is_featured == featured)
                if search:
                    search_term = f"%{search}%"
                    query = query.filter(
                        (Product.name.ilike(search_term)) |
                        (Product.description.ilike(search_term))
                    )

                total = query.count()
                products = query.offset((page - 1) * limit).limit(limit).all()

                return {
                    "products": [self._product_to_dict(p) for p in products],
                    "total": total,
                    "page": page,
                    "limit": limit,
                    "pages": (total + limit - 1) // limit
                }
            except Exception as e:
                logger.error(f"List products failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to fetch products")
            finally:
                session.close()

        @self.app.get("/api/products/{product_id}")
        async def get_product(product_id: int):
            """Get product details by ID."""
            cache_key = f"product:{product_id}"
            cached = await self.cache.get(cache_key)
            if cached:
                return json.loads(cached)

            session = self.db.get_session()
            try:
                product = session.query(Product).filter(Product.id == product_id, Product.is_active == True).first()
                if not product:
                    raise HTTPException(status_code=404, detail="Product not found")

                result = self._product_to_dict(product)
                await self.cache.set(cache_key, json.dumps(result), ttl=300)

                return result
            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Get product failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to fetch product")
            finally:
                session.close()

        @self.app.post("/api/products", status_code=status.HTTP_201_CREATED)
        async def create_product(product: PydanticProduct):
            """Create a new product."""
            session = self.db.get_session()
            try:
                if product.sku:
                    existing = session.query(Product).filter(Product.sku == product.sku).first()
                    if existing:
                        raise HTTPException(status_code=400, detail="SKU already exists")

                db_product = Product(
                    name=product.name,
                    description=product.description,
                    price=product.price,
                    currency=product.currency,
                    category=product.category,
                    subcategory=product.subcategory,
                    sku=product.sku,
                    stock_quantity=product.stock_quantity,
                    low_stock_threshold=product.low_stock_threshold,
                    is_active=product.is_active,
                    is_featured=product.is_featured,
                    images=product.images,
                    attributes=product.attributes,
                    tags=product.tags
                )
                session.add(db_product)
                session.commit()
                session.refresh(db_product)

                await self.cache.invalidate_pattern("products:*")

                return self._product_to_dict(db_product)
            except HTTPException:
                raise
            except Exception as e:
                session.rollback()
                logger.error(f"Create product failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to create product")
            finally:
                session.close()

        @self.app.put("/api/products/{product_id}")
        async def update_product(product_id: int, product: PydanticProduct):
            """Update an existing product."""
            session = self.db.get_session()
            try:
                db_product = session.query(Product).filter(Product.id == product_id).first()
                if not db_product:
                    raise HTTPException(status_code=404, detail="Product not found")

                for key, value in product.model_dump(exclude_unset=True).items():
                    if value is not None and hasattr(db_product, key):
                        setattr(db_product, key, value)

                session.commit()
                await self.cache.delete(f"product:{product_id}")
                await self.cache.invalidate_pattern("products:*")

                return self._product_to_dict(db_product)
            except HTTPException:
                raise
            except Exception as e:
                session.rollback()
                logger.error(f"Update product failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to update product")
            finally:
                session.close()

        @self.app.delete("/api/products/{product_id}")
        async def delete_product(product_id: int):
            """Soft delete a product."""
            session = self.db.get_session()
            try:
                product = session.query(Product).filter(Product.id == product_id).first()
                if not product:
                    raise HTTPException(status_code=404, detail="Product not found")

                product.is_active = False
                session.commit()

                await self.cache.delete(f"product:{product_id}")
                await self.cache.invalidate_pattern("products:*")

                return {"message": "Product deleted successfully"}
            except HTTPException:
                raise
            except Exception as e:
                session.rollback()
                logger.error(f"Delete product failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to delete product")
            finally:
                session.close()

        @self.app.get("/api/categories")
        async def list_categories():
            """List all categories."""
            session = self.db.get_session()
            try:
                categories = session.query(Category).filter(Category.is_active == True).order_by(Category.sort_order).all()
                return {"categories": [{"id": c.id, "name": c.name, "slug": c.slug, "description": c.description} for c in categories]}
            except Exception as e:
                logger.error(f"List categories failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to fetch categories")
            finally:
                session.close()

        @self.app.post("/api/cart/items")
        async def add_to_cart(item: PydanticCartItem, user_id: int):
            """Add item to shopping cart."""
            session = self.db.get_session()
            try:
                product = session.query(Product).filter(Product.id == item.product_id, Product.is_active == True).first()
                if not product:
                    raise HTTPException(status_code=404, detail="Product not found")

                if product.stock_quantity < item.quantity:
                    raise HTTPException(status_code=400, detail="Insufficient stock")

                cart = session.query(Cart).filter(Cart.user_id == user_id).first()
                if not cart:
                    cart = Cart(user_id=user_id, items=[])
                    session.add(cart)

                cart_items = cart.items or []
                found = False
                for ci in cart_items:
                    if ci.get("product_id") == item.product_id:
                        ci["quantity"] += item.quantity
                        ci["price"] = product.price
                        found = True
                        break

                if not found:
                    cart_items.append({
                        "product_id": item.product_id,
                        "quantity": item.quantity,
                        "price": product.price,
                        "name": product.name
                    })

                cart.items = cart_items
                session.commit()

                return {"cart": self._cart_to_dict(cart), "message": "Item added to cart"}
            except HTTPException:
                raise
            except Exception as e:
                session.rollback()
                logger.error(f"Add to cart failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to add item to cart")
            finally:
                session.close()

        @self.app.get("/api/cart")
        async def get_cart(user_id: int):
            """Get shopping cart."""
            session = self.db.get_session()
            try:
                cart = session.query(Cart).filter(Cart.user_id == user_id).first()
                if not cart:
                    return {"cart": {"items": [], "total": 0}, "items": []}

                cart_data = self._cart_to_dict(cart)

                enriched_items = []
                for item in cart.items or []:
                    product = session.query(Product).filter(Product.id == item.get("product_id")).first()
                    if product:
                        enriched_items.append({
                            "product_id": item.get("product_id"),
                            "name": product.name,
                            "price": item.get("price"),
                            "quantity": item.get("quantity"),
                            "subtotal": item.get("price") * item.get("quantity"),
                            "in_stock": product.stock_quantity >= item.get("quantity")
                        })

                return {"cart": cart_data, "items": enriched_items}
            except Exception as e:
                logger.error(f"Get cart failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to fetch cart")
            finally:
                session.close()

        @self.app.post("/api/orders")
        async def create_order(order: PydanticOrder):
            """Create a new order."""
            session = self.db.get_session()
            try:
                user = session.query(User).filter(User.id == order.user_id).first()
                if not user:
                    raise HTTPException(status_code=404, detail="User not found")

                order_number = f"ORD-{datetime.utcnow().strftime('%Y%m%d%H%M%S')}-{random.randint(1000, 9999)}"

                db_order = Order(
                    order_number=order_number,
                    user_id=order.user_id,
                    status="pending",
                    items=order.items,
                    subtotal=order.subtotal,
                    tax=order.tax,
                    shipping_cost=order.shipping_cost,
                    discount=order.discount,
                    total=order.total,
                    currency=order.currency,
                    shipping_address=order.shipping_address,
                    billing_address=order.billing_address or order.shipping_address
                )
                session.add(db_order)

                for item in order.items or []:
                    product = session.query(Product).filter(Product.id == item.get("product_id")).first()
                    if product:
                        product.stock_quantity = max(0, product.stock_quantity - item.get("quantity", 0))

                session.commit()
                session.refresh(db_order)

                cart = session.query(Cart).filter(Cart.user_id == order.user_id).first()
                if cart:
                    cart.items = []
                    session.commit()

                await self.cache.invalidate_pattern("products:*")

                return {
                    "order": self._order_to_dict(db_order),
                    "message": "Order created successfully"
                }
            except HTTPException:
                raise
            except Exception as e:
                session.rollback()
                logger.error(f"Create order failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to create order")
            finally:
                session.close()

        @self.app.get("/api/orders/{order_id}")
        async def get_order(order_id: int):
            """Get order details."""
            session = self.db.get_session()
            try:
                order = session.query(Order).filter(Order.id == order_id).first()
                if not order:
                    raise HTTPException(status_code=404, detail="Order not found")

                return self._order_to_dict(order)
            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Get order failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to fetch order")
            finally:
                session.close()

        @self.app.get("/api/orders/user/{user_id}")
        async def get_user_orders(user_id: int):
            """Get all orders for a user."""
            session = self.db.get_session()
            try:
                orders = session.query(Order).filter(Order.user_id == user_id).order_by(Order.created_at.desc()).all()
                return {"orders": [self._order_to_dict(o) for o in orders]}
            except Exception as e:
                logger.error(f"Get user orders failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to fetch orders")
            finally:
                session.close()

        @self.app.post("/api/payments/create-intent")
        async def create_payment(order_id: int):
            """Create Stripe payment intent for an order."""
            session = self.db.get_session()
            try:
                order = session.query(Order).filter(Order.id == order_id).first()
                if not order:
                    raise HTTPException(status_code=404, detail="Order not found")

                if order.payment_status == "paid":
                    raise HTTPException(status_code=400, detail="Order already paid")

                result = self.payment.create_payment_intent(
                    amount=order.total,
                    currency=order.currency.lower(),
                    metadata={"order_id": order.id, "order_number": order.order_number}
                )

                if result.get("id"):
                    order.payment_intent_id = result["id"]
                    session.commit()

                return result
            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Create payment intent failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to create payment")
            finally:
                session.close()

        @self.app.post("/api/payments/webhook")
        async def stripe_webhook(request: Request):
            """Handle Stripe webhook events."""
            try:
                payload = await request.body()
                sig = request.headers.get("stripe-signature")

                if not self.payment._client:
                    return JSONResponse({"error": "Stripe not configured"}, status_code=400)

                event = self.payment._client.Webhook.construct_event(
                    payload, sig, self.manager.get("stripe_webhook_secret", "")
                )

                if event["type"] == "payment_intent.succeeded":
                    intent = event["data"]["object"]
                    session = self.db.get_session()
                    try:
                        order = session.query(Order).filter(Order.payment_intent_id == intent.id).first()
                        if order:
                            order.payment_status = "paid"
                            order.status = "processing"
                            session.commit()
                    finally:
                        session.close()

                return JSONResponse({"status": "success"})
            except Exception as e:
                logger.error(f"Webhook error: {e}")
                return JSONResponse({"error": str(e)}, status_code=400)

        @self.app.post("/api/payments/refund")
        async def refund_payment(order_id: int, amount: Optional[float] = None):
            """Process a refund for an order."""
            session = self.db.get_session()
            try:
                order = session.query(Order).filter(Order.id == order_id).first()
                if not order:
                    raise HTTPException(status_code=404, detail="Order not found")

                if order.payment_status != "paid" or not order.payment_intent_id:
                    raise HTTPException(status_code=400, detail="Order has no payment to refund")

                result = self.payment.create_refund(order.payment_intent_id, amount)

                if result.get("id"):
                    order.payment_status = "refunded"
                    order.status = "refunded"
                    session.commit()

                return result
            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Refund failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to process refund")
            finally:
                session.close()

        @self.app.get("/api/shipping/rates")
        async def get_shipping_rates(weight: float, destination: Optional[str] = None):
            """Calculate shipping rates."""
            session = self.db.get_session()
            try:
                rates = session.query(ShippingRate).filter(
                    ShippingRate.is_active == True,
                    ShippingRate.min_weight <= weight,
                    (ShippingRate.max_weight >= weight) | (ShippingRate.max_weight.is_(None))
                ).all()

                result = []
                for rate in rates:
                    cost = rate.base_rate + (rate.per_unit_rate * weight)
                    result.append({
                        "carrier": rate.carrier,
                        "service": rate.service,
                        "cost": round(cost, 2),
                        "currency": rate.currency,
                        "estimated_days": f"{rate.estimated_days_min}-{rate.estimated_days_max}" if rate.estimated_days_min else "N/A"
                    })

                if not result:
                    default_rate = self.manager.get("products.default_shipping_cost", 5.99)
                    result.append({
                        "carrier": "Standard",
                        "service": "Ground",
                        "cost": default_rate,
                        "currency": "USD",
                        "estimated_days": "5-7"
                    })

                return {"rates": result}
            except Exception as e:
                logger.error(f"Get shipping rates failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to calculate shipping")
            finally:
                session.close()

        @self.app.post("/api/coupons/validate")
        async def validate_coupon(code: str, order_amount: float):
            """Validate a coupon code."""
            session = self.db.get_session()
            try:
                coupon = session.query(Coupon).filter(
                    Coupon.code == code,
                    Coupon.is_active == True,
                    Coupon.valid_from <= datetime.utcnow(),
                    Coupon.valid_until >= datetime.utcnow()
                ).first()

                if not coupon:
                    raise HTTPException(status_code=404, detail="Coupon not found or expired")

                if coupon.usage_limit and coupon.used_count >= coupon.usage_limit:
                    raise HTTPException(status_code=400, detail="Coupon usage limit reached")

                if order_amount < coupon.min_order_amount:
                    raise HTTPException(
                        status_code=400,
                        detail=f"Minimum order amount is {coupon.min_order_amount}"
                    )

                discount = coupon.discount_value
                if coupon.discount_type == "percentage":
                    discount = order_amount * (coupon.discount_value / 100)
                    if coupon.max_discount_amount:
                        discount = min(discount, coupon.max_discount_amount)
                elif coupon.discount_type == "fixed":
                    discount = coupon.discount_value

                return {
                    "valid": True,
                    "code": coupon.code,
                    "discount_type": coupon.discount_type,
                    "discount_value": coupon.discount_value,
                    "discount_amount": round(discount, 2)
                }
            except HTTPException:
                raise
            except Exception as e:
                logger.error(f"Validate coupon failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to validate coupon")
            finally:
                session.close()

        @self.app.get("/api/analytics/sales")
        async def get_sales_analytics(
            start_date: Optional[str] = None,
            end_date: Optional[str] = None
        ):
            """Get sales analytics data."""
            session = self.db.get_session()
            try:
                query = session.query(Order).filter(Order.payment_status == "paid")

                if start_date:
                    query = query.filter(Order.created_at >= datetime.fromisoformat(start_date))
                if end_date:
                    query = query.filter(Order.created_at <= datetime.fromisoformat(end_date))

                orders = query.all()

                total_revenue = sum(o.total for o in orders)
                total_orders = len(orders)
                avg_order_value = total_revenue / total_orders if total_orders > 0 else 0

                status_counts = {}
                for order in orders:
                    status_counts[order.status] = status_counts.get(order.status, 0) + 1

                return {
                    "total_revenue": round(total_revenue, 2),
                    "total_orders": total_orders,
                    "average_order_value": round(avg_order_value, 2),
                    "orders_by_status": status_counts,
                    "period": {
                        "start": start_date,
                        "end": end_date
                    }
                }
            except Exception as e:
                logger.error(f"Analytics failed: {e}")
                raise HTTPException(status_code=500, detail="Failed to fetch analytics")
            finally:
                session.close()

    def _product_to_dict(self, product: Product) -> Dict[str, Any]:
        """Convert Product model to dictionary."""
        return {
            "id": product.id,
            "name": product.name,
            "description": product.description,
            "price": product.price,
            "currency": product.currency,
            "category": product.category,
            "subcategory": product.subcategory,
            "sku": product.sku,
            "stock_quantity": product.stock_quantity,
            "low_stock_threshold": product.low_stock_threshold,
            "is_active": product.is_active,
            "is_featured": product.is_featured,
            "images": product.images or [],
            "attributes": product.attributes or {},
            "tags": product.tags or [],
            "in_stock": product.stock_quantity > 0,
            "created_at": product.created_at.isoformat() if product.created_at else None,
            "updated_at": product.updated_at.isoformat() if product.updated_at else None
        }

    def _cart_to_dict(self, cart: Cart) -> Dict[str, Any]:
        """Convert Cart model to dictionary."""
        items = cart.items or []
        total = sum(item.get("price", 0) * item.get("quantity", 0) for item in items)
        return {
            "id": cart.id,
            "user_id": cart.user_id,
            "items": items,
            "item_count": len(items),
            "subtotal": round(total, 2),
            "coupon_code": cart.coupon_code,
            "discount_amount": cart.discount_amount,
            "total": round(total - cart.discount_amount, 2)
        }

    def _order_to_dict(self, order: Order) -> Dict[str, Any]:
        """Convert Order model to dictionary."""
        return {
            "id": order.id,
            "order_number": order.order_number,
            "user_id": order.user_id,
            "status": order.status,
            "items": order.items or [],
            "subtotal": order.subtotal,
            "tax": order.tax,
            "shipping_cost": order.shipping_cost,
            "discount": order.discount,
            "total": order.total,
            "currency": order.currency,
            "shipping_address": order.shipping_address,
            "billing_address": order.billing_address,
            "payment_method": order.payment_method,
            "payment_status": order.payment_status,
            "tracking_number": order.tracking_number,
            "shipping_carrier": order.shipping_carrier,
            "created_at": order.created_at.isoformat() if order.created_at else None,
            "updated_at": order.updated_at.isoformat() if order.updated_at else None
        }

    def run(self, host: Optional[str] = None, port: Optional[int] = None, debug: bool = False):
        """Run the FastAPI server."""
        if self.app is None:
            logger.error("FastAPI application not initialized")
            return

        host = host or self.manager.get("host", "0.0.0.0")
        port = port or self.manager.get("port", 8000)
        debug = debug or self.manager.get("debug", False)

        logger.info(f"Starting server on {host}:{port}")
        uvicorn.run(self.app, host=host, port=port, debug=debug)


def main():
    """Main entry point for the e-commerce application."""
    import argparse

    parser = argparse.ArgumentParser(description="openlab E-commerce Platform")
    parser.add_argument("--manager", "-c", default="manager.yaml", help="Path to configuration file")
    parser.add_argument("--host", "-h", default=None, help="Server host")
    parser.add_argument("--port", "-p", type=int, default=None, help="Server port")
    parser.add_argument("--debug", "-d", action="store_true", help="Enable debug mode")
    args = parser.parse_args()

    app = EcommerceApp(config_path=args.manager)
    app.run(host=args.host, port=args.port, debug=args.debug)


if __name__ == "__main__":
    main()
