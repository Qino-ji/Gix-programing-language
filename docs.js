import { createHighlighter } from 'https://cdn.jsdelivr.net/npm/shiki@1.29.2/dist/index.browser.mjs';

const PAGE_ORDER = [
    'function', 'class', 'variables',
    'enum', 'struct', 'generic', 'community'
];

const PAGE_LABELS = {
    'function': 'Function',
    'class': 'Class',
    'variables': 'Variables',
    'enum': 'Enum',
    'struct': 'Struct Stmt',
    'generic': 'Generic',
    'community': 'Community & Support'
};

const pages = {};
PAGE_ORDER.forEach(p => { pages[p] = `docs/${p}.html`; });

const mdCache = {};
let allLoaded = false;
let scrollObserver = null;
let isScrollingTo = false;
let currentPageIndex = 0;

const htmlEl = document.documentElement;

// --- Theme ---
const savedTheme = localStorage.getItem('vix-theme') || 'dark';
htmlEl.setAttribute('data-theme', savedTheme);

document.getElementById('themeToggle').addEventListener('click', () => {
    const next = htmlEl.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
    htmlEl.setAttribute('data-theme', next);
    localStorage.setItem('vix-theme', next);
    showToast(next === 'light' ? '☀️ Light mode' : '🌙 Dark mode');
});

let vixGrammar = null;

async function initVixGrammar() {
    await vscodeOniguruma.loadWASM(
        await fetch('https://unpkg.com/vscode-oniguruma@2.0.1/release/onig.wasm')
    );

    const registry = new vscodeTextmate.Registry({
        onigLib: Promise.resolve({
            createOnigScanner: patterns => new vscodeOniguruma.OnigScanner(patterns),
            createOnigString: s => new vscodeOniguruma.OnigString(s),
        }),
        loadGrammar: async () => {
            const res = await fetch('syntax/vix.json');
            return await res.json();
        },
    });

    vixGrammar = await registry.loadGrammar('source.vix');
}

function tokenizeWithGrammar(code) {
    let ruleStack = vscodeTextmate.INITIAL;
    return code.split('\n').map(line => {
        const result = vixGrammar.tokenizeLine(line, ruleStack);
        ruleStack = result.ruleStack;

        let html = '';
        result.tokens.forEach((token, i) => {
            const text = line.slice(token.startIndex, result.tokens[i + 1]?.startIndex ?? line.length);
            const scope = token.scopes.at(-1) ?? '';
            const cls = scopeToClass(scope);
            const escaped = text.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
            html += cls ? `<span class="${cls}">${escaped}</span>` : escaped;
        });
        return html;
    }).join('\n');
}

function scopeToClass(scope) {
    if (scope.startsWith('comment'))                return 'hljs-comment';
    if (scope.startsWith('string'))                 return 'hljs-string';
    if (scope.startsWith('constant.numeric'))       return 'hljs-number';
    if (scope.startsWith('keyword.control'))        return 'hljs-keyword';
    if (scope.startsWith('storage.type'))           return 'hljs-built_in';
    if (scope.startsWith('support.type'))           return 'hljs-type';
    if (scope.startsWith('variable.other.enum'))    return 'hljs-built_in';
    if (scope.startsWith('entity.name.function'))   return 'hljs-title function_';
    if (scope.startsWith('keyword.operator'))       return 'hljs-operator';
    return '';
}


let shiki = null;

async function initShiki() {
    if (shiki) return;
    shiki = await createHighlighter({
        themes: ['github-dark'],
        langs: [
            'c', 'rust', 'toml', 'bash',
            async () => {
                const res = await fetch('syntax/vix.json');
                return await res.json();
            }
        ],
    });
}

async function highlightCode() {
    await initShiki();
    document.querySelectorAll('.code-block:not([data-hl]) code').forEach(block => {
        block.parentElement.setAttribute('data-hl', '1');
        const langMap = { v: 'vix', clang: 'c', sh: 'bash' };
        const lang = langMap[block.parentElement.getAttribute('data-lang')] 
                     || block.parentElement.getAttribute('data-lang');
        try {
            block.parentElement.outerHTML = shiki.codeToHtml(block.textContent, {
                lang,
                theme: 'github-dark',
            });
        } catch {
            // unsupported lang, leave as-is
        }
    });
}

function addCopyButtons() {
    document.querySelectorAll('.code-block:not([data-cp])').forEach(block => {
        block.setAttribute('data-cp', '1');
        const btn = document.createElement('button');
        btn.className = 'copy-btn';
        btn.title = 'Copy';
        btn.innerHTML = `
            <svg class="copy-icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round">
                <rect x="9" y="9" width="13" height="13" rx="2" ry="2"/>
                <path d="M5 15H4a2 2 0 0 1-2-2V4a2 2 0 0 1 2-2h9a2 2 0 0 1 2 2v1"/>
            </svg>
            <svg class="check-icon" width="16" height="16" viewBox="0 0 24 24" fill="none" stroke="currentColor" stroke-width="2" stroke-linecap="round" stroke-linejoin="round" style="display:none;">
                <polyline points="20 6 9 17 4 12"/>
            </svg>`;
        btn.onclick = () => {
            navigator.clipboard.writeText(block.querySelector('code').innerText).then(() => {
                btn.querySelector('.copy-icon').style.display = 'none';
                btn.querySelector('.check-icon').style.display = 'block';
                btn.style.color = '#10b981';
                setTimeout(() => {
                    btn.querySelector('.copy-icon').style.display = 'block';
                    btn.querySelector('.check-icon').style.display = 'none';
                    btn.style.color = '';
                }, 2000);
            });
        };
        block.appendChild(btn);
    });
}

// --- Page loading ---
function getBasePath() {
    const h = window.location.href.split('?')[0].split('#')[0];
    return h.substring(0, h.lastIndexOf('/') + 1);
}

function htmlToText(html) {
    return (html || '')
        .replace(/<script[\s\S]*?<\/script>/gi, ' ')
        .replace(/<style[\s\S]*?<\/style>/gi, ' ')
        .replace(/<[^>]+>/g, ' ')
        .replace(/\s+/g, ' ')
        .trim();
}

async function loadAllPages() {
    const ca = document.getElementById('contentArea');
    ca.innerHTML = '<div class="loading"><div class="spinner"></div><span>Loading Documentation...</span></div>';
    const base = getBasePath();
    const results = await Promise.allSettled(
        PAGE_ORDER.map(p => fetch(base + pages[p]).then(r => r.ok ? r.text() : null).catch(() => null))
    );
    ca.innerHTML = '';
    PAGE_ORDER.forEach((pageId, i) => {
        const html = results[i].status === 'fulfilled' ? results[i].value : null;
        if (html) mdCache[pageId] = htmlToText(html);
        const sec = document.createElement('div');
        sec.className = 'version-section section';
        sec.setAttribute('data-page', pageId);
        sec.innerHTML = `<span class="version-anchor" id="anchor-${pageId}"></span>` + (html || `
            <h2>${pageId.replace(/-/g, ' ').replace(/\b\w/g, c => c.toUpperCase())}</h2>
            <p style="color:var(--text-muted)">This page hasn't been written yet. Check back soon!</p>`);
        ca.appendChild(sec);
    });
    highlightCode();
    addCopyButtons();
    allLoaded = true;
    setupScrollObserver();
}

// --- Scroll / navigation ---
function setupScrollObserver() {
    if (scrollObserver) scrollObserver.disconnect();
    const navH = parseInt(getComputedStyle(document.documentElement).getPropertyValue('--nav-height-lg')) || 80;
    scrollObserver = new IntersectionObserver((entries) => {
        if (isScrollingTo) return;
        entries.forEach(entry => {
            if (entry.isIntersecting) {
                const pid = entry.target.getAttribute('data-page');
                if (pid) { currentPageIndex = PAGE_ORDER.indexOf(pid); setActivePage(pid, false); }
            }
        });
    }, { root: null, rootMargin: `-${navH + 10}px 0px -60% 0px`, threshold: 0 });
    document.querySelectorAll('.version-section').forEach(sec => scrollObserver.observe(sec));
}

function setActivePage(pageId, updateHistory = true) {
    document.querySelectorAll('.sidebar-link').forEach(link => {
        link.classList.toggle('active', link.getAttribute('data-page') === pageId);
    });
    if (updateHistory) history.replaceState({ page: pageId }, '', '#' + pageId);
}

function scrollToPage(pageId) {
    const anchor = document.getElementById('anchor-' + pageId);
    if (!anchor) return;
    isScrollingTo = true;
    currentPageIndex = PAGE_ORDER.indexOf(pageId);
    setActivePage(pageId, true);
    const navH = parseInt(getComputedStyle(document.documentElement).getPropertyValue('--nav-height-lg')) || 80;
    const top = anchor.getBoundingClientRect().top + window.scrollY - navH - 8;
    window.scrollTo({ top, behavior: 'smooth' });
    setTimeout(() => { isScrollingTo = false; }, 800);
}

// --- Progress rail ---
const progressRail  = document.getElementById('progressRail');
const progressFill  = document.getElementById('progressFill');
const progressThumb = document.getElementById('progressThumb');
const progressTip   = document.getElementById('progressTooltip');
const progressLabel = document.getElementById('progressLabel');
const progressPctEl = document.getElementById('progressPct');
const backToTopBtn  = document.getElementById('backToTop');

function updateProgress() {
    const scrollTop = window.scrollY;
    const docHeight = document.documentElement.scrollHeight - window.innerHeight;
    const pct = docHeight > 0 ? scrollTop / docHeight : 0;
    const pctPx = pct * 100;

    progressFill.style.height = pctPx + '%';

    const trackEl = document.getElementById('progressTrack');
    const thumbTop = pct * trackEl.offsetHeight;
    progressThumb.style.top = thumbTop + 'px';
    progressTip.style.top = thumbTop + 'px';
    progressPctEl.textContent = Math.round(pctPx) + '%';

    const activeLink = document.querySelector('.sidebar-link.active');
    if (activeLink) progressLabel.textContent = activeLink.textContent.trim();

    backToTopBtn.classList.toggle('visible', scrollTop > 300);
}

let isDragging = false;
function railJump(e) {
    const track = document.getElementById('progressTrack');
    const rect = track.getBoundingClientRect();
    const y = (e.clientY || (e.touches && e.touches[0].clientY) || 0) - rect.top;
    const pct = Math.max(0, Math.min(1, y / rect.height));
    window.scrollTo({ top: pct * (document.documentElement.scrollHeight - window.innerHeight), behavior: 'auto' });
}

progressRail.addEventListener('mousedown', e => { isDragging = true; progressRail.classList.add('dragging'); railJump(e); e.preventDefault(); });
document.addEventListener('mousemove', e => { if (isDragging) railJump(e); });
document.addEventListener('mouseup', () => { isDragging = false; progressRail.classList.remove('dragging'); });

window.addEventListener('scroll', () => {
    document.body.classList.toggle('scrolled', window.scrollY > 50);
    updateProgress();
}, { passive: true });

backToTopBtn.addEventListener('click', () => window.scrollTo({ top: 0, behavior: 'smooth' }));

// --- Toast ---
let kbToastTimer = null;
function showToast(html) {
    const toast = document.getElementById('kbToast');
    toast.innerHTML = html;
    toast.classList.add('show');
    clearTimeout(kbToastTimer);
    kbToastTimer = setTimeout(() => toast.classList.remove('show'), 2000);
}

// --- Keyboard shortcuts ---
document.addEventListener('keydown', e => {
    const tag = document.activeElement.tagName;
    if (tag === 'INPUT' || tag === 'TEXTAREA') return;

    if (e.key === 'j' || e.key === 'J') {
        if (!allLoaded) return;
        const next = Math.min(currentPageIndex + 1, PAGE_ORDER.length - 1);
        if (next !== currentPageIndex) { scrollToPage(PAGE_ORDER[next]); showToast(`<kbd>J</kbd> → ${PAGE_LABELS[PAGE_ORDER[next]]}`); }
    } else if (e.key === 'k' || e.key === 'K') {
        if (!allLoaded) return;
        const prev = Math.max(currentPageIndex - 1, 0);
        if (prev !== currentPageIndex) { scrollToPage(PAGE_ORDER[prev]); showToast(`<kbd>K</kbd> → ${PAGE_LABELS[PAGE_ORDER[prev]]}`); }
    } else if (e.key === 's' || e.key === 'S') {
        document.getElementById('sidebar').classList.toggle('collapsed');
        showToast('<kbd>S</kbd> Sidebar toggled');
    } else if (e.key === '/') {
        e.preventDefault();
        document.getElementById('searchInput').focus();
        showToast('<kbd>/</kbd> Search');
    } else if (e.key === 'g' || e.key === 'G') {
        window.scrollTo({ top: 0, behavior: 'smooth' });
        showToast('<kbd>G</kbd> Top');
    } else if (e.key === 't' || e.key === 'T') {
        const next = htmlEl.getAttribute('data-theme') === 'dark' ? 'light' : 'dark';
        htmlEl.setAttribute('data-theme', next);
        localStorage.setItem('vix-theme', next);
        showToast(next === 'light' ? '<kbd>T</kbd> ☀️ Light' : '<kbd>T</kbd> 🌙 Dark');
    } else if (e.key === '?') {
        showToast('<kbd>J</kbd>/<kbd>K</kbd> nav · <kbd>S</kbd> sidebar · <kbd>T</kbd> theme · <kbd>/</kbd> search · <kbd>G</kbd> top');
    }
});

// --- Search ---
const searchInput    = document.getElementById('searchInput');
const searchDropdown = document.getElementById('searchDropdown');
const searchClear    = document.getElementById('searchClear');

function escapeRegex(s) { return s.replace(/[.*+?^${}()|[\]\\]/g, '\\$&'); }

function getSnippet(text, query, maxLen = 90) {
    const idx = text.toLowerCase().indexOf(query.toLowerCase());
    if (idx === -1) return text.slice(0, maxLen) + '…';
    const start = Math.max(0, idx - 30), end = Math.min(text.length, idx + query.length + 60);
    let snippet = (start > 0 ? '…' : '') + text.slice(start, end) + (end < text.length ? '…' : '');
    snippet = snippet.replace(/&/g, '&amp;').replace(/</g, '&lt;').replace(/>/g, '&gt;');
    return snippet.replace(new RegExp(`(${escapeRegex(query)})`, 'gi'), '<mark>$1</mark>');
}

function closeSearch() {
    searchDropdown.classList.remove('open');
    searchInput.value = '';
    searchClear.style.display = 'none';
    document.querySelectorAll('.sidebar-link').forEach(l => l.classList.remove('filtered-out'));
    document.querySelectorAll('.sidebar-category').forEach(c => c.classList.remove('filtered-out'));
}

function runSearch(query) {
    searchDropdown.innerHTML = '';
    if (!query || query.length < 2) {
        searchDropdown.classList.remove('open');
        document.querySelectorAll('.sidebar-link').forEach(l => l.classList.remove('filtered-out'));
        document.querySelectorAll('.sidebar-category').forEach(c => c.classList.remove('filtered-out'));
        return;
    }
    searchDropdown.classList.add('open');
    const q = query.toLowerCase();
    const results = PAGE_ORDER
        .filter(pageId => (PAGE_LABELS[pageId] || pageId).toLowerCase().includes(q) || (mdCache[pageId] || '').toLowerCase().includes(q))
        .map(pageId => ({ pageId, label: PAGE_LABELS[pageId] || pageId, md: mdCache[pageId] || '' }));

    document.querySelectorAll('.sidebar-link').forEach(link => {
        link.classList.toggle('filtered-out', !results.some(r => r.pageId === link.getAttribute('data-page')));
    });
    document.querySelectorAll('.sidebar-section').forEach(section => {
        const cat = section.previousElementSibling;
        const has = [...section.querySelectorAll('.sidebar-link')].some(l => !l.classList.contains('filtered-out'));
        if (cat && cat.classList.contains('sidebar-category')) cat.classList.toggle('filtered-out', !has);
    });

    if (results.length === 0) {
        searchDropdown.innerHTML = `<div class="search-no-results">No results for "${query}"</div>`;
        return;
    }
    results.slice(0, 8).forEach(r => {
        const item = document.createElement('div');
        item.className = 'search-result-item';
        item.innerHTML = `<div class="search-result-version">${r.label}</div>
            <div class="search-result-snippet">${r.md ? getSnippet(r.md, query) : r.label}</div>`;
        item.addEventListener('mousedown', e => {
            e.preventDefault();
            closeSearch();
            if (allLoaded) { scrollToPage(r.pageId); }
            else { const t = setInterval(() => { if (allLoaded) { clearInterval(t); scrollToPage(r.pageId); } }, 100); }
        });
        searchDropdown.appendChild(item);
    });
}

searchInput.addEventListener('input', () => {
    const v = searchInput.value;
    searchClear.style.display = v ? 'block' : 'none';
    runSearch(v);
});
searchClear.addEventListener('mousedown', e => { e.preventDefault(); searchInput.value = ''; searchClear.style.display = 'none'; runSearch(''); });
document.addEventListener('mousedown', e => { if (!document.getElementById('searchContainer').contains(e.target)) searchDropdown.classList.remove('open'); });
searchInput.addEventListener('keydown', e => { if (e.key === 'Escape') { searchDropdown.classList.remove('open'); searchInput.blur(); } });

// --- Sidebar ---
document.getElementById('sidebar').addEventListener('click', e => {
    const link = e.target.closest('.sidebar-link');
    if (!link || !allLoaded) return;
    const pageId = link.getAttribute('data-page');
    if (pageId) scrollToPage(pageId);
});

document.getElementById('sidebarToggle').addEventListener('click', () => {
    document.getElementById('sidebar').classList.toggle('collapsed');
});

document.addEventListener('DOMContentLoaded', async () => {
    await loadAllPages();
    const hash = window.location.hash.replace('#', '');
    setTimeout(() => scrollToPage(hash && PAGE_ORDER.includes(hash) ? hash : PAGE_ORDER[0]), 100);
    updateProgress();
});

window.addEventListener('popstate', e => {
    const pageId = (e.state && e.state.page) || window.location.hash.replace('#', '');
    if (pageId && allLoaded) scrollToPage(pageId);
});