#!/usr/bin/env bash
# Configura automaticamente o repositorio Aletheia no GitHub:
# topics, GitHub Pages (branch gh-pages), Discussions + anuncio de lancamento,
# labels uteis e descricao/homepage do repo.
#
# Requisito: ficheiro .dsh-token na raiz do repo com um fine-grained PAT:
#   https://github.com/settings/tokens?tokenType=beta
#   Repository access: ONLY "MarielCarlosFrias/Aletheia"
#   Permissoes: Contents RW, Metadata R, Issues RW, Pull requests RW,
#               Discussions RW, Pages RW, Administration RW
#   Guarda:  echo 'ghp_...' > .dsh-token
# O token nunca e impresso nem commitado (.git/info/exclude).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
TOKEN_FILE="$ROOT/.dsh-token"
if [ ! -f "$TOKEN_FILE" ]; then
  echo "ERRO: falta $TOKEN_FILE" >&2
  echo "  echo 'ghp_...' > $TOKEN_FILE   (fine-grained PAT, so este repo)" >&2
  exit 1
fi
TOKEN="$(tr -d '[:space:]' < "$TOKEN_FILE")"
REPO="MarielCarlosFrias/Aletheia"
API="https://api.github.com/repos/$REPO"
GQL="https://api.github.com/graphql"
UA="aletheia-gh-setup/1.0"

api() { curl -sS -H "Authorization: Bearer $TOKEN" -H "Accept: application/vnd.github+json" \
        -H "X-GitHub-Api-Version: 2022-11-28" -H "User-Agent: $UA" "$@"; }
gql() { curl -sS -X POST -H "Authorization: Bearer $TOKEN" -H "User-Agent: $UA" \
        "$GQL" -d "$1"; }

echo "== 1/6 topics =="
api -X PUT "$API/topics" \
  -d '{"names":["qt6","qt","onnxruntime","onnx","background-removal","image-processing","image-segmentation","cpp","cpp17","desktop-app","privacy","offline-first","rembg","pixel-art","open-source"]}' \
  | python3 -c "import sys,json;d=json.load(sys.stdin);print('topics:', ', '.join(d.get('names',[])))" || true

echo "== 2/6 GitHub Pages (branch gh-pages) =="
api -X PUT "$API/pages" -d '{"source":{"branch":"gh-pages","path":"/"}}' >/dev/null 2>&1 \
  || api -X PATCH "$API/pages" -d '{"source":{"branch":"gh-pages","path":"/"}}' >/dev/null 2>&1 || true
api "$API/pages" | python3 -c "import sys,json;d=json.load(sys.stdin);print('pages:', d.get('html_url') or d)" || true

echo "== 3/6 descricao + homepage do repo =="
api -X PATCH "$API" -d '{"description":"Private, offline background removal for images — Qt 6, ONNX Runtime, batch processing and manual mask refinement.","homepage":"https://marielcarlosfrias.github.io/Aletheia/"}' \
  | python3 -c "import sys,json;d=json.load(sys.stdin);print('homepage:', d.get('homepage'))" || true

echo "== 4/6 ativar Discussions =="
RID="$(api "$API" | python3 -c "import sys,json;print(json.load(sys.stdin)['node_id'])")"
gql "$(python3 -c 'import json,sys;print(json.dumps({"query":"mutation($id:ID!){updateRepository(input:{repositoryId:$id,hasDiscussionsEnabled:true}){repository{hasDiscussionsEnabled}}}","variables":{"id":sys.argv[1]}}))' "$RID")" \
  | head -c 200; echo

echo "== 5/6 anuncio de lancamento (Discussions) =="
CAT="$(gql "$(python3 -c 'import json;print(json.dumps({"query":"query{repository(owner:\"MarielCarlosFrias\",name:\"Aletheia\"){discussionCategories(first:20){nodes{id,name}}}}","variables":{}}))')" \
  | python3 -c "import sys,json;d=json.load(sys.stdin);cats=(d.get('data') or {}).get('repository',{}).get('discussionCategories',{}).get('nodes',[]);print(next((c['id'] for c in cats if c['name'].lower()=='announcements'), cats[0]['id'] if cats else ''))" 2>/dev/null || true)"
if [ -n "$CAT" ]; then
  BODY='🚀 **Aletheia v1.4.3 is out!**

Aletheia is a **100% offline, open-source (MIT)** desktop app that removes image backgrounds locally with AI — native **C++17 + Qt 6** with **ONNX Runtime** (U²-Net, BRIA RMBG 1.4/2.0, BiRefNet, ISNet, all SHA256-verified).

- 🔒 No cloud, no accounts, no telemetry — your photos never leave your device
- 🤖 One-click model catalog with SHA256 integrity verification
- 🎨 Chroma key / pixel-art mode (exact color, tolerance, contiguous flood-fill)
- ✂️ Sticker outline, drop shadow, auto-crop, edge refinement + live preview
- 🖌️ Manual mask brush with undo/redo
- 📁 Batch processing (GUI + `aletheia_cli`) with parallel workers
- 🪟🍎🐧 Pre-built binaries: Windows zip, macOS universal2, Linux AppImage/.deb

👉 **Download:** https://github.com/MarielCarlosFrias/Aletheia/releases/latest
👉 **Landing page:** https://marielcarlosfrias.github.io/Aletheia/

CI is green on Linux, macOS and Windows. Feedback, issues and PRs very welcome! ⭐'
  gql "$(python3 -c 'import json,sys;print(json.dumps({"query":"mutation($rid:ID!,$cid:ID!,$t:String!,$b:String!){createDiscussion(input:{repositoryId:$rid,categoryId:$cid,title:$t,body:$b}){discussion{url}}}","variables":{"rid":sys.argv[1],"cid":sys.argv[2],"t":"🚀 Aletheia v1.4.3 — 100% offline AI background remover (Qt 6 + ONNX Runtime)","b":sys.argv[3]}}))' "$RID" "$CAT" "$BODY")" \
    | python3 -c "import sys,json;d=json.load(sys.stdin);print('discussion:', (d.get('data') or {}).get('createDiscussion',{}).get('discussion',{}).get('url') or d)" || true
else
  echo "  (sem categoria de Discussions — ativar Discussions em Settings primeiro)"
fi

echo "== 6/6 labels uteis =="
while IFS='|' read -r name color desc; do
  [ -z "$name" ] && continue
  api -X POST "$API/labels" -d "$(python3 -c 'import json,sys;print(json.dumps({"name":sys.argv[1],"color":sys.argv[2],"description":sys.argv[3]}))' "$name" "$color" "$desc")" >/dev/null 2>&1 || true
done <<'EOF'
bug|d73a4a|Something isn't working
enhancement|0e8a7d|New feature or request
good first issue|7057ff|Good for newcomers
help wanted|a9e6b3|Extra attention is needed
documentation|0075ca|Improvements or additions to documentation
divulgacao|5319e7|Marketing / promotion
question|d876e3|Further information is requested
EOF
api "$API/labels?per_page=100" | python3 -c "import sys,json;print('labels:', ', '.join(l['name'] for l in json.load(sys.stdin)))" || true

echo "== feito =="
