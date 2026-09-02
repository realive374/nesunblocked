"""Inject the shared question bank into the study page template."""
import json
import os

from questions import VOCAB, build

HERE = os.path.dirname(os.path.abspath(__file__))
TEMPLATE = os.path.join(HERE, "study_page_template.html")
OUT = os.path.join(HERE, "vocab-buzzer.html")

with open(TEMPLATE) as f:
    html = f.read()

terms = json.dumps(VOCAB["terms"], indent=2)
bank = json.dumps(
    [{k: q[k] for k in ("round", "prompt", "term", "answers", "correct")} for q in build()],
    indent=2,
)

for marker, payload in (("/*__TERMS__*/", terms), ("/*__BANK__*/", bank)):
    if marker not in html:
        raise SystemExit("marker %s missing from template" % marker)
    html = html.replace(marker, payload)

with open(OUT, "w") as f:
    f.write(html)

print("wrote", OUT)
