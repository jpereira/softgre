'use strict';
const search = document.querySelector('#search');
const sections = [...document.querySelectorAll('.doc-section')];
const normalize = (text) => text.normalize('NFD').replace(/[\u0300-\u036f]/g, '').toLowerCase();
const content = new Map(sections.map((section) => [section, normalize(section.textContent)]));
search?.addEventListener('input', () => {
  const query = normalize(search.value.trim());
  let matches = 0;
  for (const section of sections) {
    section.hidden = !content.get(section).includes(query);
    if (!section.hidden) matches++;
  }
  document.querySelector('.empty').hidden = matches > 0;
});
for (const link of document.querySelectorAll('.sidebar nav a')) {
  link.addEventListener('click', () => {
    search.value = '';
    search.dispatchEvent(new Event('input'));
  });
}
if (navigator.clipboard?.writeText) {
  for (const button of document.querySelectorAll('.copy')) {
    button.classList.add('available');
    button.addEventListener('click', async () => {
      const code = button.closest('.code-block').querySelector('pre code').textContent;
      try {
        await navigator.clipboard.writeText(code);
        button.textContent = document.body.dataset.copied;
        setTimeout(() => { button.textContent = document.body.dataset.copy; }, 1800);
      } catch {
        button.textContent = document.body.dataset.copy;
      }
    });
  }
}
