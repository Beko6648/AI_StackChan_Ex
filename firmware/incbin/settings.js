document.addEventListener('DOMContentLoaded', function () {
  const offsetX = document.getElementById('offsetX');
  const offsetY = document.getElementById('offsetY');
  const saveOffsetButton = document.getElementById('saveOffsetButton');
  const statusDiv = document.getElementById('status');
  const errorDiv  = document.getElementById('error');

  function showStatus(msg) { statusDiv.textContent = msg; errorDiv.textContent = ''; }
  function showError(msg)  { errorDiv.textContent = msg;  statusDiv.textContent = ''; }

  // 現在のオフセット値を取得
  fetch('/servo_offset')
    .then(async (res) => {
      if (!res.ok) throw new Error('Failed to get servo offset');
      const data = await res.json();
      offsetX.value = data.x;
      offsetY.value = data.y;
    })
    .catch((err) => showError('Error loading offset: ' + err.message));

  // 保存
  saveOffsetButton.addEventListener('click', () => {
    const x = parseInt(offsetX.value);
    const y = parseInt(offsetY.value);
    if (isNaN(x) || isNaN(y)) { showError('Please enter valid numbers.'); return; }
    const body = JSON.stringify({ x: x, y: y });
    fetch('/servo_offset', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: body
    })
      .then(async (res) => {
        if (!res.ok) throw new Error(await res.text());
        showStatus('Saved. Please reboot to apply.');
      })
      .catch((err) => showError('Error saving offset: ' + err.message));
  });
});
