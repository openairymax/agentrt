import React from 'react';

interface InputProps {
  value: string;
  onChange: (e: React.ChangeEvent<HTMLInputElement>) => void;
  placeholder?: string;
  type?: string;
  disabled?: boolean;
  className?: string;
  style?: React.CSSProperties;
  onKeyDown?: (e: React.KeyboardEvent<HTMLInputElement>) => void;
}

export const Input: React.FC<InputProps> = ({
  value,
  onChange,
  placeholder,
  type = 'text',
  disabled = false,
  className = '',
  style = {},
  onKeyDown,
}) => {
  return (
    <input
      type={type}
      value={value}
      onChange={onChange}
      placeholder={placeholder}
      disabled={disabled}
      className={className}
      style={{
        width: '100%',
        padding: '8px 12px',
        border: '1px solid var(--border-color)',
        borderRadius: '6px',
        backgroundColor: 'var(--bg-tertiary)',
        color: 'var(--text-primary)',
        fontSize: '12px',
        fontFamily: 'inherit',
        transition: 'all 0.2s ease',
        ...style,
        opacity: disabled ? 0.5 : 1,
        cursor: disabled ? 'not-allowed' : 'text',
      }}
      onKeyDown={onKeyDown}
      onFocus={(e) => {
        e.currentTarget.style.borderColor = 'var(--primary-color)';
        e.currentTarget.style.boxShadow = '0 0 0 3px var(--primary-light)';
      }}
      onBlur={(e) => {
        e.currentTarget.style.borderColor = 'var(--border-color)';
        e.currentTarget.style.boxShadow = 'none';
      }}
    />
  );
};
