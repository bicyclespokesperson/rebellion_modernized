'use strict';
const $ = (id) => document.getElementById(id);
const BATCH_SIZE = 10;
const gradeNames = {correct: 'Looks correct', missing: 'Missing features', extra: 'Extra background', both: 'Both problems'};
let review;
let page = 0;
let busy = false;

async function request(url, options = {}) {
  const response = await fetch(url, options);
  if (!response.ok) throw new Error(`Request failed (${response.status}). Your last action was not confirmed saved.`);
  return response.json();
}

function showError(error) {
  $('error').textContent = error.message;
  $('error').hidden = false;
}

function batches() {
  const result = [];
  for (let index = 0; index < review.items.length; index += BATCH_SIZE) result.push(review.items.slice(index, index + BATCH_SIZE));
  return result;
}

function pageKey() { return `encyclopedia-review-page:${review.dataset}:${review.scope}`; }

function statistics() {
  const completed = batches().filter(batch => batch.every(item => review.grades[item.id]));
  const sample = completed.flat();
  const bad = sample.filter(item => review.grades[item.id] !== 'correct').length;
  const n = sample.length;
  if (!n) return {n, bad};
  const proportion = bad / n;
  if (n === review.items.length) return {n, bad, proportion, low: proportion, high: proportion};
  const z = 1.95996398454;
  const denominator = 1 + z * z / n;
  const center = (proportion + z * z / (2 * n)) / denominator;
  const half = z * Math.sqrt(proportion * (1 - proportion) / n + z * z / (4 * n * n)) / denominator;
  return {n, bad, proportion, low: Math.max(0, center - half), high: Math.min(1, center + half)};
}

function refreshSummary() {
  const count = review.items.length;
  const graded = review.items.filter(item => review.grades[item.id]);
  const flagged = graded.filter(item => review.grades[item.id] !== 'correct');
  const stats = statistics();
  const percent = value => `${(value * 100).toFixed(1)}%`;
  $('reviewed').textContent = `${graded.length} / ${count}`;
  $('population').textContent = 'Unique encyclopedia images in this set';
  $('flagged').textContent = flagged.length;
  $('rate').textContent = stats.n ? percent(stats.proportion) : '—';
  $('interval').textContent = stats.n ? (stats.n === count ? 'Full set reviewed · measured rate' : `95% Wilson interval: ${percent(stats.low)}–${percent(stats.high)}`) : 'Finish a batch to calculate';
  $('affected').textContent = stats.n ? `≈ ${Math.round(stats.proportion * count)} / ${count}` : '—';
  $('estimate-note').textContent = stats.n ? `Range: ${Math.floor(stats.low * count)}–${Math.ceil(stats.high * count)} images in this set` : 'Within the selected population';
  $('method').textContent = `Fixed random order, sampled without replacement. Estimate uses ${stats.n} images from fully graded batches; ${graded.length - stats.n} grades in partial batches are pending. ${stats.n && stats.n < 30 ? 'Small sample: the uncertainty is still wide. ' : ''}Each review set has independent grades. “All icon categories” excludes mission artwork. These are full cutouts, before icon cropping and palette conversion.`;
  const current = batches()[page] || [];
  const done = current.filter(item => review.grades[item.id]).length;
  $('batch-count').textContent = `${done} / ${current.length} graded in this batch`;
  $('page-label').textContent = count ? `Batch ${page + 1} of ${batches().length}` : 'No images available';
  $('page-progress').textContent = done === current.length ? 'Batch complete' : `${current.length - done} still to review`;
  $('previous').disabled = busy || page === 0;
  $('next').disabled = busy || page >= batches().length - 1 || done !== current.length;
  $('scope').disabled = busy;
  $('export').disabled = busy;
  $('save-note').textContent = `Grades save automatically to ${review.save_path}. The order and grades resume when you return. Click a selected grade again to clear it.`;
}

function figure(item, kind, zoom = false) {
  const node = document.createElement('figure');
  const image = document.createElement('img');
  image.alt = `${item.label} — ${kind === 'source' ? 'original encyclopedia artwork' : 'extracted subject'}`;
  image.src = `/images/${encodeURIComponent(item.id)}/${kind}.png`;
  image.addEventListener('error', () => {
    showError(new Error(`Could not load ${item.id}/${kind}.png. Reload before grading this batch.`));
    document.querySelectorAll(`[data-id="${item.id}"] .grades button`).forEach(button => button.disabled = true);
  });
  const caption = document.createElement('figcaption');
  caption.textContent = kind === 'source' ? 'ENCYCLOPEDIA ORIGINAL' : 'EXTRACTED SUBJECT';
  if (zoom) {
    const background = document.createElement('div');
    background.className = kind === 'cutout' ? 'cutout' : '';
    background.append(image);
    node.append(background);
  } else {
    const button = document.createElement('button');
    button.className = `image-button ${kind === 'cutout' ? 'cutout' : ''}`;
    button.setAttribute('aria-label', `Enlarge comparison for ${item.label}`);
    button.append(image);
    button.addEventListener('click', () => {
      $('zoom-title').textContent = `${item.label} · ${item.id}`;
      $('zoom-pair').replaceChildren(figure(item, 'source', true), figure(item, 'cutout', true));
      $('zoom').showModal();
    });
    node.append(button);
  }
  node.append(caption);
  return node;
}

function updateCard(card, item) {
  const grade = review.grades[item.id];
  card.classList.toggle('correct', grade === 'correct');
  card.classList.toggle('flagged', Boolean(grade && grade !== 'correct'));
  card.querySelectorAll('.grades button').forEach(button => button.setAttribute('aria-pressed', String(button.dataset.grade === grade)));
  card.querySelector('.card-status').textContent = grade ? `✓ Saved · ${gradeNames[grade]}` : 'Awaiting your review';
}

async function gradeItem(card, item, grade) {
  if (busy) return;
  busy = true;
  refreshSummary();
  card.querySelector('.card-status').textContent = 'Saving…';
  $('error').hidden = true;
  const value = review.grades[item.id] === grade ? null : grade;
  try {
    await request('/api/grade', {method: 'POST', headers: {'Content-Type': 'application/json'}, body: JSON.stringify({dataset: review.dataset, scope: review.scope, id: item.id, grade: value})});
    if (value) review.grades[item.id] = value;
    else delete review.grades[item.id];
  } catch (error) { showError(error); }
  finally {
    busy = false;
    updateCard(card, item);
    refreshSummary();
  }
}

function render() {
  const cards = (batches()[page] || []).map((item, index) => {
    const card = document.createElement('article');
    card.className = 'card';
    card.dataset.id = item.id;
    card.innerHTML = '<div class="card-head"><div><h2></h2><div class="identity"></div></div><span class="number"></span></div><div class="pair"></div><div class="grades" role="group"></div><div class="card-status" aria-live="polite"></div>';
    card.querySelector('h2').textContent = item.label;
    card.querySelector('.identity').textContent = `${item.id} · ${item.category}`;
    card.querySelector('.number').textContent = String(page * BATCH_SIZE + index + 1).padStart(2, '0');
    card.querySelector('.pair').append(figure(item, 'source'), figure(item, 'cutout'));
    const group = card.querySelector('.grades');
    group.setAttribute('aria-label', `Grade ${item.label}`);
    for (const [grade, label] of Object.entries(gradeNames)) {
      const button = document.createElement('button');
      button.textContent = label;
      button.dataset.grade = grade;
      button.addEventListener('click', () => gradeItem(card, item, grade));
      group.append(button);
    }
    updateCard(card, item);
    return card;
  });
  $('cards').replaceChildren(...cards);
  refreshSummary();
  try { localStorage.setItem(pageKey(), String(page)); } catch { /* Disk grades still persist. */ }
}

async function loadScope() {
  if (busy) return;
  busy = true;
  $('scope').disabled = true;
  $('cards').inert = true;
  try {
    review = await request(`/api/review?scope=${encodeURIComponent($('scope').value)}`);
    let savedPage = NaN;
    try { savedPage = Number.parseInt(localStorage.getItem(pageKey()), 10); } catch { /* Resume at the first incomplete batch. */ }
    const firstIncomplete = batches().findIndex(batch => batch.some(item => !review.grades[item.id]));
    page = Number.isInteger(savedPage) ? Math.max(0, Math.min(savedPage, batches().length - 1)) : Math.max(0, firstIncomplete);
    $('error').hidden = true;
    busy = false;
    render();
  } catch (error) {
    if (review) $('scope').value = review.scope;
    showError(error);
  }
  finally { busy = false; $('scope').disabled = false; $('cards').inert = false; }
}

$('scope').addEventListener('change', loadScope);
$('background').addEventListener('change', () => document.body.dataset.background = $('background').value);
$('previous').addEventListener('click', () => { if (!busy && page > 0) { page--; render(); window.scrollTo({top: 0}); } });
$('next').addEventListener('click', () => { if (!busy && !$('next').disabled) { page++; render(); window.scrollTo({top: 0}); } });
$('close-zoom').addEventListener('click', () => $('zoom').close());
$('zoom').addEventListener('click', event => { if (event.target === $('zoom')) $('zoom').close(); });
$('export').addEventListener('click', async () => {
  try {
    const data = await request('/api/export');
    const url = URL.createObjectURL(new Blob([JSON.stringify(data, null, 2)], {type: 'application/json'}));
    const link = document.createElement('a');
    link.href = url;
    link.download = `encyclopedia-review-${review.dataset.slice(0, 16)}.json`;
    link.click();
    setTimeout(() => URL.revokeObjectURL(url), 1000);
  } catch (error) { showError(error); }
});
loadScope();
