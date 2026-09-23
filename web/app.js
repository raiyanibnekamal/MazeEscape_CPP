const canvas = document.querySelector('#maze');
const ctx = canvas.getContext('2d');
const $ = (selector) => document.querySelector(selector);
const difficulties = [{ rows: 5, label: 'Small' }, { rows: 7, label: 'Medium' }, { rows: 9, label: 'Large' }];
const itemPool = [{ name: 'Torch', value: 10 }, { name: 'Health Potion', value: 15 }, { name: 'Golden Key', value: 50 }, { name: 'Shield', value: 20 }, { name: 'Map Fragment', value: 25 }];
const enemyNames = ['Goblin', 'Skeleton', 'Shadow Wraith'];
const colors = { cell: '#2a2420', visited: '#241e1a', current: '#e7a43b', exit: '#e7c45e', hint: '#758f68', danger: '#a6423d', wall: '#a88b66', text: '#f3e8d0', muted: '#a99b83' };
const game = { started: false, running: false, name: '', rows: 5, adj: [], exit: 24, room: 0, health: 100, score: 0, turn: 0, items: new Map(), enemies: [], inventory: [], history: [], visited: new Set(), hintPath: [], log: [] };

function id(x, y) { return y * game.rows + x; }
function xy(room) { return [room % game.rows, Math.floor(room / game.rows)]; }
function neighbors(room) { return [...game.adj[room]]; }
function addEdge(a, b) { game.adj[a].add(b); game.adj[b].add(a); }
function generateMaze() {
  const count = game.rows * game.rows;
  game.adj = Array.from({ length: count }, () => new Set());
  const visited = new Set([0]);
  const stack = [0];
  while (stack.length) {
    const current = stack[stack.length - 1];
    const [x, y] = xy(current);
    const options = [[x - 1, y], [x + 1, y], [x, y - 1], [x, y + 1]]
      .filter(([nx, ny]) => nx >= 0 && nx < game.rows && ny >= 0 && ny < game.rows && !visited.has(id(nx, ny)));
    if (!options.length) { stack.pop(); continue; }
    const next = options[Math.floor(Math.random() * options.length)];
    const nextRoom = id(next[0], next[1]);
    addEdge(current, nextRoom); visited.add(nextRoom); stack.push(nextRoom);
  }
  let extra = Math.floor(count / 5);
  let guard = 0;
  while (extra && guard++ < 2000) {
    const a = Math.floor(Math.random() * count), [ax, ay] = xy(a);
    const candidates = [[ax - 1, ay], [ax + 1, ay], [ax, ay - 1], [ax, ay + 1]].filter(([nx, ny]) => nx >= 0 && nx < game.rows && ny >= 0 && ny < game.rows);
    if (candidates.length) {
      const next = candidates[Math.floor(Math.random() * candidates.length)], b = id(next[0], next[1]);
      if (!game.adj[a].has(b)) { addEdge(a, b); extra--; }
    }
  }
}
function bfsPath(start, goal) {
  const queue = [start], parent = new Map([[start, -1]]);
  while (queue.length) {
    const current = queue.shift();
    if (current === goal) break;
    neighbors(current).forEach((next) => { if (!parent.has(next)) { parent.set(next, current); queue.push(next); } });
  }
  if (!parent.has(goal)) return [];
  const path = []; for (let current = goal; current !== -1; current = parent.get(current)) path.push(current);
  return path.reverse();
}
function dijkstraStep(start, target) {
  const distance = Array(game.adj.length).fill(Infinity), parent = Array(game.adj.length).fill(-1), open = [{ room: start, distance: 0 }];
  distance[start] = 0;
  while (open.length) {
    open.sort((a, b) => a.distance - b.distance);
    const current = open.shift();
    if (current.room === target) break;
    neighbors(current.room).forEach((next) => { const nextDistance = current.distance + 1; if (nextDistance < distance[next]) { distance[next] = nextDistance; parent[next] = current.room; open.push({ room: next, distance: nextDistance }); } });
  }
  if (!Number.isFinite(distance[target])) return start;
  let current = target;
  while (parent[current] !== start && parent[current] !== -1) current = parent[current];
  return current;
}
function logAction(message) { game.log.unshift(message); game.log = game.log.slice(0, 6); renderInfo(); }
function startGame(name, difficulty) {
  game.name = name || 'Explorer'; game.rows = difficulties[difficulty].rows; game.exit = game.rows * game.rows - 1;
  game.room = 0; game.health = 100; game.score = 0; game.turn = 0; game.items = new Map(); game.enemies = []; game.inventory = []; game.history = []; game.visited = new Set([0]); game.hintPath = []; game.log = [];
  generateMaze();
  const used = new Set();
  itemPool.forEach((item) => { let room; do room = 1 + Math.floor(Math.random() * (game.exit - 1)); while (used.has(room)); used.add(room); game.items.set(room, item); });
  enemyNames.forEach((name) => { let room; do room = 1 + Math.floor(Math.random() * (game.exit - 1)); while (room === game.exit); game.enemies.push({ name, room, power: 10 }); });
  game.started = true; game.running = true; $('#overlay').classList.add('hidden'); logAction(`A new maze awaits, ${game.name}.`); render();
}
function moveEnemies() {
  game.enemies.forEach((enemy) => { enemy.room = dijkstraStep(enemy.room, game.room); if (enemy.room === game.room) { game.health -= enemy.power; logAction(`${enemy.name} catches you! -${enemy.power} HP`); } });
  game.health = Math.max(0, game.health); if (!game.health) finish(false);
}
function finish(won) { game.running = false; if (won) { game.score += 100; logAction('You escaped the maze! +100 bonus!'); } else logAction('The darkness caught up with you.'); render(); showEndDialog(won); }
function move(direction) {
  if (!game.running) return;
  const [x, y] = xy(game.room), delta = { n: [0, -1], s: [0, 1], e: [1, 0], w: [-1, 0] }[direction];
  const nx = x + delta[0], ny = y + delta[1];
  if (nx < 0 || nx >= game.rows || ny < 0 || ny >= game.rows) { logAction("There's a wall that way."); return; }
  const target = id(nx, ny);
  if (!game.adj[game.room].has(target)) { logAction("There's a wall that way."); return; }
  game.history.push(game.room); game.room = target; game.visited.add(target); game.hintPath = []; game.turn++;
  logAction(`Moved to room ${target}.`); moveEnemies(); if (game.running && game.room === game.exit) finish(true); render();
}
function pickup() {
  if (!game.running) return;
  const item = game.items.get(game.room); if (!item) { logAction('Nothing to pick up here.'); return; }
  game.inventory.push(item.name); game.score += item.value; game.items.delete(game.room); logAction(`Picked up ${item.name} (+${item.value})`); render();
}
function hint() {
  if (!game.running) return;
  game.hintPath = bfsPath(game.room, game.exit); game.score = Math.max(0, game.score - 5); logAction(`Hint: shortest path is ${game.hintPath.length - 1} steps. (-5 score)`); render();
}
function undo() { if (!game.running) return; if (!game.history.length) { logAction('Nothing to undo.'); return; } game.room = game.history.pop(); game.hintPath = []; logAction(`Undid move, back to room ${game.room}.`); render(); }
function render() {
  const size = canvas.width, pad = 16, cell = (size - pad * 2) / game.rows;
  ctx.clearRect(0, 0, size, size); ctx.fillStyle = '#0c0a09'; ctx.fillRect(0, 0, size, size);
  for (let room = 0; room < game.rows * game.rows; room++) {
    const [x, y] = xy(room), left = pad + x * cell + 3, top = pad + y * cell + 3, width = cell - 6;
    const enemy = game.enemies.find((entry) => entry.room === room), isCurrent = room === game.room, isHint = game.hintPath.includes(room);
    ctx.fillStyle = isCurrent ? colors.current : enemy ? colors.danger : isHint ? colors.hint : game.visited.has(room) ? colors.visited : colors.cell;
    ctx.beginPath(); ctx.roundRect(left, top, width, width, 8); ctx.fill();
    if (room === game.exit) { ctx.strokeStyle = colors.exit; ctx.setLineDash([5, 4]); ctx.lineWidth = 2; ctx.stroke(); ctx.setLineDash([]); }
    if (isHint || enemy || isCurrent) { ctx.strokeStyle = isHint ? colors.exit : enemy ? '#d56b5a' : '#f4c66a'; ctx.lineWidth = 2; ctx.stroke(); }
    ctx.strokeStyle = colors.wall; ctx.lineWidth = 4;
    const right = game.adj[room].has(id(x + 1, y)), down = game.adj[room].has(id(x, y + 1));
    if (x < game.rows - 1 && !right) { ctx.beginPath(); ctx.moveTo(left + width, top); ctx.lineTo(left + width, top + width); ctx.stroke(); }
    if (y < game.rows - 1 && !down) { ctx.beginPath(); ctx.moveTo(left, top + width); ctx.lineTo(left + width, top + width); ctx.stroke(); }
    ctx.fillStyle = isCurrent ? '#17100a' : colors.text; ctx.textAlign = 'center'; ctx.textBaseline = 'middle'; ctx.font = `${Math.max(12, cell * .18)}px 'DM Mono'`;
    if (isCurrent) ctx.fillText('@', left + width / 2, top + width / 2); else if (enemy) ctx.fillText('X', left + width / 2, top + width / 2); else if (room === game.exit) ctx.fillText('EXIT', left + width / 2, top + width / 2); else if (game.visited.has(room) && game.items.has(room)) ctx.fillText('✦', left + width / 2, top + width / 2);
  }
  renderInfo();
}
function renderInfo() {
  $('#health').textContent = game.started ? game.health : '100'; $('#score').textContent = game.started ? game.score : '0'; $('#room').textContent = game.started ? game.room : '--'; $('#difficulty').textContent = game.started ? `${game.rows} × ${game.rows}` : '--'; $('#turn-count').textContent = `TURN ${game.turn}`;
  $('#health-bar').style.width = `${game.health}%`; $('#health-bar').style.background = game.health < 35 ? '#b94d43' : '#e7a43b'; $('#status').textContent = game.running ? `${game.name}'s expedition` : game.started ? 'Expedition ended' : 'Ready for a new expedition';
  $('#inventory').innerHTML = game.inventory.length ? game.inventory.map((item) => `<li>${item}</li>`).join('') : '<li>Empty</li>'; $('#log').innerHTML = game.log.map((entry) => `<li>${entry}</li>`).join('');
  $('#pickup').disabled = !game.running || !game.items.has(game.room); $('#hint').disabled = !game.running; $('#undo').disabled = !game.running || !game.history.length;
}
function showEndDialog(won) { $('#dialog-title').textContent = won ? 'You escaped!' : 'The maze wins'; $('#dialog-copy').textContent = `Final score: ${game.score}. Start another expedition when you are ready.`; $('#begin').innerHTML = 'Play again <span>→</span>'; $('#start-form').dataset.end = 'true'; $('#overlay').classList.remove('hidden'); $('#player-name').value = game.name; }
$('#start-form').addEventListener('submit', (event) => { event.preventDefault(); startGame($('#player-name').value.trim(), Number(document.querySelector('input[name="difficulty"]:checked').value)); });
$('#new-game').addEventListener('click', () => { $('#start-form').dataset.end = ''; $('#dialog-title').textContent = 'Maze Escape'; $('#dialog-copy').textContent = 'Choose a size, enter your name, and find the sunlit room.'; $('#begin').innerHTML = 'Begin expedition <span>→</span>'; $('#overlay').classList.remove('hidden'); });
$('#pickup').addEventListener('click', pickup); $('#hint').addEventListener('click', hint); $('#undo').addEventListener('click', undo); document.querySelectorAll('[data-move]').forEach((button) => button.addEventListener('click', () => move(button.dataset.move)));
document.addEventListener('keydown', (event) => { const direction = { ArrowUp: 'n', w: 'n', ArrowDown: 's', s: 's', ArrowLeft: 'w', a: 'w', ArrowRight: 'e', d: 'e' }[event.key]; if (direction) { event.preventDefault(); move(direction); } if (event.key.toLowerCase() === 'p') pickup(); if (event.key.toLowerCase() === 'h') hint(); if (event.key.toLowerCase() === 'u') undo(); });
renderInfo(); render();
