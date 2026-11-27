#!/usr/bin/env bash
# Publica posts de divulgacao do Aletheia usando tokens de APLICACAO
# (nunca passwords de conta!). Configurar em .dsh-social.env (git-excluido):
#
#   MASTODON_INSTANCE=https://mastodon.social
#   MASTODON_ACCESS_TOKEN=...        # Mastodon: Preferences -> Development -> New application
#                                    # (scope: write:statuses) -> copia "Your access token"
#   REDDIT_CLIENT_ID=...
#   REDDIT_CLIENT_SECRET=...
#   REDDIT_REFRESH_TOKEN=...         # Reddit: cria app "script" em https://www.reddit.com/prefs/apps,
#                                    # depois abre https://www.reddit.com/api/v1/authorize?client_id=...&response_type=code&state=x&redirect_uri=<redirect_uri_da_app>&duration=permanent&scope=submit
#                                    # e troca o "code" por refresh_token:
#                                    #   curl -u ID:SECRET -d grant_type=authorization_code -d code=CODE -d redirect_uri=URI https://www.reddit.com/api/v1/access_token
#   REDDIT_USER_AGENT=aletheia-divulgacao/1.0 by u/TEU_USERNAME
#
# Uso:
#   scripts/social-post.sh mastodon "texto do post"
#   scripts/social-post.sh reddit <subreddit> "titulo" "corpo do self post"
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
ENVF="$ROOT/.dsh-social.env"
if [ ! -f "$ENVF" ]; then
  echo "ERRO: falta $ENVF (ve o cabecalho deste script para como obter os tokens)" >&2
  exit 1
fi
set -a; . "$ENVF"; set +a

cmd="${1:?uso: social-post.sh <mastodon|reddit> ...}"; shift

case "$cmd" in
  mastodon)
    text="${1:?texto em falta}"
    : "${MASTODON_INSTANCE:?defina MASTODON_INSTANCE}" "${MASTODON_ACCESS_TOKEN:?defina MASTODON_ACCESS_TOKEN}"
    curl -sS -X POST "https://${MASTODON_INSTANCE#https://}/api/v1/statuses" \
      -H "Authorization: Bearer $MASTODON_ACCESS_TOKEN" \
      --data-urlencode "status=$text" \
      -d "visibility=public" \
      | python3 -c "import sys,json;d=json.load(sys.stdin);print('publicado:', d.get('url') or d)"
    ;;
  reddit)
    sub="${1:?subreddit em falta}"; title="${2:?titulo em falta}"; body="${3:-}"
    : "${REDDIT_CLIENT_ID:?}" "${REDDIT_CLIENT_SECRET:?}" "${REDDIT_REFRESH_TOKEN:?}" "${REDDIT_USER_AGENT:?}"
    tok="$(curl -sS -u "$REDDIT_CLIENT_ID:$REDDIT_CLIENT_SECRET" \
      -d "grant_type=refresh_token&refresh_token=$REDDIT_REFRESH_TOKEN" \
      -A "$REDDIT_USER_AGENT" https://www.reddit.com/api/v1/access_token \
      | python3 -c "import sys,json;print(json.load(sys.stdin)['access_token'])")"
    curl -sS -X POST https://oauth.reddit.com/api/submit \
      -H "Authorization: Bearer $tok" -A "$REDDIT_USER_AGENT" \
      --data-urlencode "sr=$sub" --data-urlencode "title=$title" \
      --data-urlencode "text=$body" -d "kind=self" -d "api_type=json" \
      | python3 -c "import sys,json;d=json.load(sys.stdin);print('publicado:', d['json']['data'].get('url') or d)"
    ;;
  *)
    echo "comando desconhecido: $cmd (usa: mastodon|reddit)" >&2; exit 2 ;;
esac
