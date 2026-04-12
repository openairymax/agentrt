import React, { useEffect, useRef, useState, useCallback } from "react";

interface AnimationConfig {
  duration?: number;
  delay?: number;
  easing?: string;
}

export const useSpringAnimation = (trigger: boolean, config: AnimationConfig = {}) => {
  const [isAnimating, setIsAnimating] = useState(false);
  const ref = useRef<HTMLDivElement>(null);

  useEffect(() => {
    if (trigger && ref.current) {
      setIsAnimating(true);
      const element = ref.current;

      element.style.transition = `all ${config.duration || 0.6}s ${config.easing || 'cubic-bezier(0.34, 1.56, 0.64, 1)'}`;
      element.style.transitionDelay = `${config.delay || 0}ms`;

      requestAnimationFrame(() => {
        element.style.transform = 'scale(1)';
        element.style.opacity = '1';
      });

      const timeout = setTimeout(() => {
        setIsAnimating(false);
      }, (config.duration || 600) + (config.delay || 0));

      return () => clearTimeout(timeout);
    }
  }, [trigger, config.duration, config.delay, config.easing]);

  return { ref, isAnimating };
};

export const useRippleEffect = () => {
  const [ripples, setRipples] = useState<{ id: number; x: number; y: number; size: number }[]>([]);

  const createRipple = useCallback((event: React.MouseEvent<HTMLElement>) => {
    const button = event.currentTarget;
    const rect = button.getBoundingClientRect();
    const size = Math.max(rect.width, rect.height) * 2.5;
    const x = event.clientX - rect.left - size / 2;
    const y = event.clientY - rect.top - size / 2;

    const id = Date.now();
    setRipples(prev => [...prev, { id, x, y, size }]);

    setTimeout(() => {
      setRipples(prev => prev.filter(r => r.id !== id));
    }, 600);
  }, []);

  return { ripples, createRipple };
};

export const useParallaxEffect = (speed: number = 0.5) => {
  const [offset, setOffset] = useState({ x: 0, y: 0 });

  useEffect(() => {
    const handleMouseMove = (e: MouseEvent) => {
      const x = (window.innerWidth / 2 - e.clientX) * speed;
      const y = (window.innerHeight / 2 - e.clientY) * speed;
      setOffset({ x, y });
    };

    window.addEventListener('mousemove', handleMouseMove);
    return () => window.removeEventListener('mousemove', handleMouseMove);
  }, [speed]);

  return offset;
};

export const useTypewriterEffect = (text: string, speed: number = 50) => {
  const [displayText, setDisplayText] = useState('');
  const [isComplete, setIsComplete] = useState(false);

  useEffect(() => {
    let index = 0;
    setDisplayText('');
    setIsComplete(false);

    const timer = setInterval(() => {
      if (index < text.length) {
        setDisplayText(text.slice(0, index + 1));
        index++;
      } else {
        setIsComplete(true);
        clearInterval(timer);
      }
    }, speed);

    return () => clearInterval(timer);
  }, [text, speed]);

  return { displayText, isComplete };
};

export const useCountUp = (target: number, duration: number = 2000, startOnMount: boolean = true) => {
  const [current, setCurrent] = useState(0);
  const [isAnimating, setIsAnimating] = useState(false);

  const animate = useCallback((to: number) => {
    setIsAnimating(true);
    const startTime = Date.now();
    const from = current;

    const step = () => {
      const now = Date.now();
      const progress = Math.min((now - startTime) / duration, 1);

      const easeOutQuart = 1 - Math.pow(1 - progress, 4);
      setCurrent(Math.floor(from + (to - from) * easeOutQuart));

      if (progress < 1) {
        requestAnimationFrame(step);
      } else {
        setCurrent(to);
        setIsAnimating(false);
      }
    };

    requestAnimationFrame(step);
  }, [current, duration]);

  useEffect(() => {
    if (startOnMount) {
      animate(target);
    }
  }, [target, startOnMount, animate]);

  return { current, isAnimating, animate };
};

export const MagneticButton: React.FC<{
  children: React.ReactNode;
  strength?: number;
  className?: string;
  style?: React.CSSProperties;
  onClick?: () => void;
}> = ({ children, strength = 0.3, className = '', style = {}, onClick }) => {
  const [position, setPosition] = useState({ x: 0, y: 0 });
  const ref = useRef<HTMLDivElement>(null);

  const handleMouseMove = (e: React.MouseEvent) => {
    if (!ref.current) return;
    const rect = ref.current.getBoundingClientRect();
    const x = (e.clientX - rect.left - rect.width / 2) * strength;
    const y = (e.clientY - rect.top - rect.height / 2) * strength;
    setPosition({ x, y });
  };

  const handleMouseLeave = () => {
    setPosition({ x: 0, y: 0 });
  };

  return (
    <div
      ref={ref}
      className={`magnetic-button ${className}`}
      style={{
        ...style,
        transform: `translate(${position.x}px, ${position.y}px)`,
        transition: 'transform 0.3s cubic-bezier(0.34, 1.56, 0.64, 1)',
      }}
      onMouseMove={handleMouseMove}
      onMouseLeave={handleMouseLeave}
      onClick={onClick}
    >
      {children}
    </div>
  );
};

export const GlowCard: React.FC<{
  children: React.ReactNode;
  glowColor?: string;
  intensity?: number;
  className?: string;
  style?: React.CSSProperties;
  hoverable?: boolean;
}> = ({
  children,
  glowColor = '#6366f1',
  intensity = 0.4,
  className = '',
  style = {},
  hoverable = true
}) => {
  const [mousePos, setMousePos] = useState({ x: 0, y: 0 });
  const cardRef = useRef<HTMLDivElement>(null);

  const handleMouseMove = (e: React.MouseEvent) => {
    if (!cardRef.current || !hoverable) return;
    const rect = cardRef.current.getBoundingClientRect();
    setMousePos({
      x: ((e.clientX - rect.left) / rect.width) * 100,
      y: ((e.clientY - rect.top) / rect.height) * 100,
    });
  };

  return (
    <div
      ref={cardRef}
      className={`glow-card ${className}`}
      style={{
        ...style,
        position: 'relative',
        overflow: 'hidden',
        background: 'var(--bg-secondary)',
        borderRadius: 'var(--radius-lg)',
        border: '1px solid var(--border-subtle)',
        transition: 'all 0.3s ease',
      }}
      onMouseMove={handleMouseMove}
    >
      <div
        style={{
          position: 'absolute',
          inset: 0,
          background: `radial-gradient(circle at ${mousePos.x}% ${mousePos.y}%, ${glowColor}${Math.round(intensity * 255).toString(16).padStart(2, '0')}, transparent 60%)`,
          opacity: hoverable ? 1 : 0,
          pointerEvents: 'none',
          transition: 'opacity 0.3s ease',
        }}
      />
      <div style={{ position: 'relative', zIndex: 1 }}>
        {children}
      </div>
    </div>
  );
};

export const GradientText: React.FC<{
  children: React.ReactNode;
  colors?: string[];
  animated?: boolean;
  className?: string;
  style?: React.CSSProperties;
}> = ({ children, colors = ['#6366f1', '#a78bfa'], animated = true, className = '', style = {} }) => (
  <span
    className={`gradient-text ${className}`}
    style={{
      ...style,
      background: `linear-gradient(135deg, ${colors.join(', ')})`,
      backgroundSize: animated ? '200% 200%' : '100%',
      WebkitBackgroundClip: 'text',
      WebkitTextFillColor: 'transparent',
      backgroundClip: 'text',
      animation: animated ? 'gradientShift 3s ease infinite' : 'none',
    }}
  >
    {children}
  </span>
);

export const FloatingElement: React.FC<{
  children: React.ReactNode;
  amplitude?: number;
  frequency?: number;
  delay?: number;
  className?: string;
  style?: React.CSSProperties;
}> = ({ children, amplitude = 10, frequency = 3, delay = 0, className = '', style = {} }) => (
  <div
    className={`floating-element ${className}`}
    style={{
      ...style,
      animation: `float ${frequency}s ease-in-out ${delay}s infinite`,
      '--float-amplitude': `${amplitude}px`,
    } as React.CSSProperties}
  >
    {children}
  </div>
);

const styles = `
  @keyframes gradientShift {
    0%, 100% { background-position: 0% 50%; }
    50% { background-position: 100% 50%; }
  }

  @keyframes float {
    0%, 100% { transform: translateY(0px); }
    50% { transform: translateY(var(--float-amplitude)); }
  }

  .magnetic-button {
    display: inline-flex;
    align-items: center;
    justify-content: center;
  }

  .glow-card:hover {
    border-color: var(--border-color);
    transform: translateY(-2px);
  }

  .gradient-text {
    font-weight: 700;
  }
`;

if (typeof document !== 'undefined') {
  const styleSheet = document.createElement('style');
  styleSheet.textContent = styles;
  document.head.appendChild(styleSheet);
}
