// The settings page opened from the Pebble phone app. It is served as a data
// URI, so there is nothing to host.

// hasKey: whether a key is already saved. The key itself is never put in the
// page, so it doesn't end up in a URL.
function configPage(hasKey, returnTo) {
  var html =
    '<!DOCTYPE html><html><head><meta charset="utf-8">' +
    '<meta name="viewport" content="width=device-width,initial-scale=1">' +
    '<title>Pebblesaur</title><style>' +
    'body{font-family:-apple-system,Roboto,sans-serif;margin:0;padding:20px;background:#f4f2fb;color:#1d1a2b}' +
    'h1{font-size:22px;margin:0 0 4px}p{color:#5b5670;font-size:14px;line-height:1.4}' +
    'label{display:block;font-weight:600;margin:20px 0 6px}' +
    'input{width:100%;box-sizing:border-box;font-size:16px;padding:12px;border:1px solid #c9c3e0;border-radius:8px}' +
    'button{width:100%;font-size:17px;padding:14px;border:0;border-radius:8px;margin-top:16px;font-weight:600}' +
    '#save{background:#5b3fd1;color:#fff}#clear{background:transparent;color:#a3302b}' +
    '.status{font-size:13px;color:#2d7a3e;margin-top:6px}' +
    '.support{margin-top:32px;padding-top:16px;border-top:1px solid #d8d3ea;text-align:center}' +
    '.support a{color:#5b3fd1;font-weight:600;text-decoration:none}' +
    '</style></head><body>' +
    '<h1>Pebblesaur</h1>' +
    '<p>Run your Liftosaur workouts from your watch. Create an API key in Liftosaur under ' +
    '<b>Settings &rarr; API Keys</b> (requires premium), then paste it here.</p>' +
    '<label for="key">Liftosaur API key</label>' +
    '<input id="key" type="password" autocomplete="off" autocapitalize="off" spellcheck="false" ' +
    'placeholder="' + (hasKey ? 'Saved. Paste a new key to replace it' : 'lftsk_...') + '">' +
    (hasKey ? '<div class="status">An API key is saved.</div>' : '') +
    '<button id="save">Save</button>' +
    (hasKey ? '<button id="clear">Remove saved key</button>' : '') +
    '<div class="support"><p>Enjoying Pebblesaur? It is free and open source.</p>' +
    '<a href="https://buymeacoffee.com/quillsh" target="_blank">&#9749; Buy me a coffee</a></div>' +
    '<script>' +
    'var returnTo=' + JSON.stringify(returnTo) + ';' +
    'function done(settings){document.location=returnTo+encodeURIComponent(JSON.stringify(settings));}' +
    'document.getElementById("save").onclick=function(){' +
    'var key=document.getElementById("key").value.trim();' +
    'if(!key){done({});return;}' +
    'if(key.indexOf("lftsk_")!==0&&!confirm("Liftosaur keys start with lftsk_. Save anyway?"))return;' +
    'done({apiKey:key});};' +
    'var clear=document.getElementById("clear");' +
    'if(clear)clear.onclick=function(){done({clearKey:true});};' +
    '</script></body></html>';
  return 'data:text/html;charset=utf-8,' + encodeURIComponent(html);
}

module.exports = {
  configPage: configPage
};
