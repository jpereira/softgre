'use strict';
(() => {
  let theme = 'light';
  try {
    if (localStorage.getItem('softgre-theme') === 'dark') theme = 'dark';
  } catch { /* Browsing without storage still supports switching themes. */ }
  document.documentElement.dataset.theme = theme;
  document.addEventListener('DOMContentLoaded', () => {
    const button = document.querySelector('#theme-toggle');
    if (!button) return;
    const update = () => {
      const dark = document.documentElement.dataset.theme === 'dark';
      button.setAttribute('aria-pressed', String(dark));
      button.textContent = dark ? button.dataset.light : button.dataset.dark;
    };
    update();
    button.hidden = false;
    button.addEventListener('click', () => {
      const next = document.documentElement.dataset.theme === 'dark' ? 'light' : 'dark';
      document.documentElement.dataset.theme = next;
      try { localStorage.setItem('softgre-theme', next); } catch { /* Storage is optional. */ }
      update();
    });
  });
})();
