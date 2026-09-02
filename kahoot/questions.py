"""Question bank for the vocabulary Kahoot.

Single source of truth for both the Kahoot import spreadsheet and the
playable study page. Each entry is (prompt, correct, [3 distractors], slot)
where `slot` is the 1-4 answer position the correct answer takes.
"""
import json
import os

HERE = os.path.dirname(os.path.abspath(__file__))

with open(os.path.join(HERE, "vocab.json")) as f:
    VOCAB = json.load(f)

DEFN = {t["term"]: t["definition"] for t in VOCAB["terms"]}

# (term, [3 distractor terms], slot for the "what does X mean" question,
#         slot for the "which word means X" question)
PLAN = [
    ("abstain",    ["accessible", "boisterous", "accede"],   2, 3),
    ("accede",     ["curtail", "cunning", "abstain"],        4, 1),
    ("accessible", ["cordial", "affluent", "diligent"],      1, 4),
    ("affluent",   ["accessible", "abstain", "boisterous"],  3, 2),
    ("animate",    ["curtail", "diligent", "cordial"],       2, 1),
    ("boisterous", ["cunning", "accessible", "accede"],      4, 3),
    ("cordial",    ["diligent", "boisterous", "abstain"],    1, 2),
    ("cunning",    ["animate", "affluent", "curtail"],       3, 4),
    ("curtail",    ["accede", "cordial", "cunning"],         4, 3),
    ("diligent",   ["boisterous", "accessible", "animate"],  2, 1),
]

# "Which word means ...?" needs its own distractors so the two rounds
# don't mirror each other exactly.
WORD_DISTRACTORS = {
    "abstain":    ["accede", "curtail", "animate"],
    "accede":     ["abstain", "accessible", "cordial"],
    "accessible": ["affluent", "accede", "diligent"],
    "affluent":   ["accessible", "boisterous", "cordial"],
    "animate":    ["abstain", "cunning", "curtail"],
    "boisterous": ["cordial", "diligent", "affluent"],
    "cordial":    ["cunning", "boisterous", "accessible"],
    "cunning":    ["cordial", "diligent", "animate"],
    "curtail":    ["accede", "animate", "abstain"],
    "diligent":   ["boisterous", "cunning", "affluent"],
}


def _place(correct, distractors, slot):
    """Put `correct` in position `slot` (1-indexed), distractors around it."""
    answers = list(distractors)
    answers.insert(slot - 1, correct)
    return answers


def build():
    """Return the full 20-question bank."""
    questions = []

    # Round 1: term -> definition
    for term, distractors, slot, _ in PLAN:
        questions.append({
            "round": "term-to-definition",
            "prompt": 'What does "%s" mean?' % term,
            "term": term,
            "answers": _place(DEFN[term], [DEFN[d] for d in distractors], slot),
            "correct": slot,
        })

    # Round 2: definition -> term
    for term, _, _, slot in PLAN:
        questions.append({
            "round": "definition-to-term",
            "prompt": 'Which word means "%s"?' % DEFN[term],
            "term": term,
            "answers": _place(term, WORD_DISTRACTORS[term], slot),
            "correct": slot,
        })

    return questions


if __name__ == "__main__":
    bank = build()
    print(json.dumps(bank, indent=2))
