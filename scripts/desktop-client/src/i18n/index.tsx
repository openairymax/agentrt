import React, { createContext, useContext, useState, useCallback, ReactNode } from 'react';
import en from './en.json';
import zh from './zh.json';

type TranslationKeys = typeof en;
type Language = 'en' | 'zh';

interface I18nContextType {
  language: Language;
  t: TranslationKeys;
  setLanguage: (lang: Language) => void;
  availableLanguages: { code: Language; name: string }[];
}

const translations: Record<Language, TranslationKeys> = {
  en,
  zh,
};

const I18nContext = createContext<I18nContextType | undefined>(undefined);

function detectSystemLanguage(): Language {
  if (typeof navigator !== 'undefined') {
    const lang = navigator.language.toLowerCase();
    if (lang.startsWith('zh')) {
      return 'zh';
    }
  }
  return 'en';
}

function getStoredLanguage(): Language | null {
  if (typeof localStorage !== 'undefined') {
    const stored = localStorage.getItem('agentos-language');
    if (stored === 'en' || stored === 'zh') {
      return stored;
    }
  }
  return null;
}

export function I18nProvider({ children }: { children: ReactNode }) {
  const [language, setLanguageState] = useState<Language>(() => {
    return getStoredLanguage() || detectSystemLanguage();
  });

  const setLanguage = useCallback((lang: Language) => {
    setLanguageState(lang);
    if (typeof localStorage !== 'undefined') {
      localStorage.setItem('agentos-language', lang);
    }
  }, []);

  const value: I18nContextType = {
    language,
    t: translations[language],
    setLanguage,
    availableLanguages: [
      { code: 'en', name: 'English' },
      { code: 'zh', name: '简体中文' },
    ],
  };

  return (
    <I18nContext.Provider value={value}>
      {children}
    </I18nContext.Provider>
  );
}

export function useI18n(): I18nContextType {
  const context = useContext(I18nContext);
  if (!context) {
    throw new Error('useI18n must be used within an I18nProvider');
  }
  return context;
}

export function useTranslation() {
  const { t } = useI18n();
  return t;
}
