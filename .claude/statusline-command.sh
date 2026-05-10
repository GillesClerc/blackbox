#!/bin/sh
input=$(cat)

used=$(echo "$input" | jq -r '.context_window.used_percentage // empty')
if [ -n "$used" ]; then
  ctx=$(printf "ctx:%.0f%%" "$used")
else
  ctx="ctx:--"
fi

five=$(echo "$input" | jq -r '.rate_limits.five_hour.used_percentage // empty')
week=$(echo "$input" | jq -r '.rate_limits.seven_day.used_percentage // empty')
limits=""
if [ -n "$five" ]; then
  limits=" | 5h:$(printf '%.0f' "$five")%"
fi
if [ -n "$week" ]; then
  limits="$limits 7d:$(printf '%.0f' "$week")%"
fi

printf "%s%s" "$ctx" "$limits"
