#!/usr/bin/env bash
# Custom keyer names. Needs a daemon in demo mode (mhuxd -2), renames its demo keyers only.
set -euo pipefail

HOST="${HOST:-127.0.0.1}"
PORT="${PORT:-5052}"

api="http://${HOST}:${PORT}/api/v1"
A=DK_DEMO_DK_1
B=D2_DEMO_DK2_1
failed=0

fail() { echo "FAIL: $1" >&2; failed=1; }

# patch SERIAL JSON EXPECTED_STATUS
patch() {
  local code
  code=$(curl -sS -o /dev/null -w '%{http_code}' -X PATCH "$api/config/devices/$1" \
         -H 'Content-Type: application/json' -d "$2")
  [ "$code" = "$3" ] || fail "PATCH $1 $2: expected $3, got $code"
}

# expect_name SERIAL NAME
expect_name() {
  case "$(curl -fsS "$api/devices")" in
    *"\"serial\":\"$1\",\"name\":\"$2\""*) ;;
    *) fail "$1 should be named '$2' in /devices";;
  esac
}

cleanup() {
  curl -sS -o /dev/null -X PATCH "$api/config/devices/$A" -d '{"name":null}' || true
  curl -sS -o /dev/null -X PATCH "$api/config/devices/$B" -d '{"name":null}' || true
}
trap cleanup EXIT
cleanup

# The default is the serial number.
expect_name "$A" "$A"
case "$(curl -fsS "$api/devices")" in
  *"\"serial\":\"$A\",\"name\":\"$A\",\"model\":\"DIGI KEYER\""*) ;;
  *) fail "/devices should report the model";;
esac

patch "$A" '{"name":"KeyerNameTest"}' 200
expect_name "$A" KeyerNameTest
case "$(curl -fsS "$api/config/devices/$A")" in
  *'"name":"KeyerNameTest"'*) ;;
  *) fail "config should have the custom name";;
esac

# Unique, regardless of case, serial numbers of other keyers included.
patch "$B" '{"name":"KeyerNameTest"}' 409
patch "$B" '{"name":"kEYERnAMEtEST"}' 409
patch "$B" "{\"name\":\"$A\"}" 409
patch "$A" "{\"name\":\"$B\"}" 409
expect_name "$B" "$B"
curl -fsS -o /dev/null -X POST "$api/config/devices" -d "{\"devices\":[{\"serial\":\"$B\",\"name\":\"$A\"}]}" 2>/dev/null \
  && fail "POST /config/devices should refuse a name in use"

# A refused name keeps the rest of the request from being applied.
patch "$B" '{"name":"KeyerNameTest","cwMessages":[{"index":1,"text":"KEYERNAMETEST"}]}' 409
case "$(curl -fsS "$api/config/devices/$B")" in
  *KEYERNAMETEST*) fail "request with a refused name should not be applied";;
esac

# Not valid.
patch "$B" '{"name":" KeyerNameTest2"}' 400
patch "$B" '{"name":"KeyerNameTest2 "}' 400
patch "$B" '{"name":"a\tb"}' 400
patch "$B" '{"name":42}' 400
patch "$B" '{"name":"123456789012345678901234567890123"}' 400
expect_name "$B" "$B"

# 32 characters, not bytes.
patch "$B" '{"name":"ÄÖÜ456789012345678901234567890 2"}' 200
expect_name "$B" 'ÄÖÜ456789012345678901234567890 2'

# Back to the default: empty, null or the own serial number. Only then the name is free again.
patch "$A" '{"name":""}' 200
expect_name "$A" "$A"
case "$(curl -fsS "$api/config/devices/$A")" in
  *'"name"'*) fail "config should not have a default name";;
esac
patch "$B" '{"name":"KeyerNameTest"}' 200
patch "$B" '{"name":null}' 200
expect_name "$B" "$B"
patch "$B" '{"name":"KeyerNameTest"}' 200
patch "$B" "{\"name\":\"$B\"}" 200
expect_name "$B" "$B"

[ "$failed" = 0 ] || exit 1
echo "OK"
