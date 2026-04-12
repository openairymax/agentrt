import { useCallback } from 'react';
import { useToast } from './Toast';
import { useModal } from './Modal';

export const useAlert = () => {
  const { addToast } = useToast();
  const { showModal } = useModal();

  const success = useCallback((title: string, message?: string) => {
    addToast({ type: 'success', title, message });
  }, [addToast]);

  const error = useCallback((title: string, message?: string) => {
    addToast({ type: 'error', title, message });
  }, [addToast]);

  const warning = useCallback((title: string, message?: string) => {
    addToast({ type: 'warning', title, message });
  }, [addToast]);

  const info = useCallback((title: string, message?: string) => {
    addToast({ type: 'info', title, message });
  }, [addToast]);

  const confirm = useCallback(async (options: {
    title: string;
    message: string;
    type?: 'confirm' | 'danger' | 'warning';
    confirmText?: string;
    cancelText?: string;
  }): Promise<boolean> => {
    return showModal(options);
  }, [showModal]);

  return { success, error, warning, info, confirm };
};
