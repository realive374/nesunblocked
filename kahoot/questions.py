"""Question bank for the vocabulary Kahoot.

Single source of truth for both the Kahoot import spreadsheet and the
playable study page. Every question shows the word and asks for its meaning.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))

with open(os.path.join(HERE, "vocab.json")) as f:
    VOCAB = json.load(f)

DEFN = {t["term"]: t["definition"] for t in VOCAB["terms"]}

# (term, 3 distractor terms, the 1-4 answer slot the correct definition takes)
PLAN = [
    ("abstain",    ["accessible", "boisterous", "accede"],   2),
    ("accede",     ["curtail", "cunning", "abstain"],        4),
    ("accessible", ["cordial", "affluent", "diligent"],      1),
    ("affluent",   ["accessible", "abstain", "boisterous"],  3),
    ("animate",    ["curtail", "diligent", "cordial"],       2),
    ("boisterous", ["cunning", "accessible", "accede"],      4),
    ("cordial",    ["diligent", "boisterous", "abstain"],    1),
    ("cunning",    ["animate", "affluent", "curtail"],       3),
    ("curtail",    ["accede", "cordial", "cunning"],         4),
    ("diligent",   ["boisterous", "accessible", "animate"],  2),
]


def _place(correct, distractors, slot):
    """Put `correct` in position `slot` (1-indexed), distractors around it."""
    answers = list(distractors)
    answers.insert(slot - 1, correct)
    return answers


def build():
    """Return the question bank: one word-to-meaning question per term."""
    return [
        {
            "prompt": 'What does "%s" mean?' % term,
            "term": term,
            "answers": _place(DEFN[term], [DEFN[d] for d in distractors], slot),
            "correct": slot,
        }
        for term, distractors, slot in PLAN
    ]


if __name__ == "__main__":
    print(json.dumps(build(), indent=2))
