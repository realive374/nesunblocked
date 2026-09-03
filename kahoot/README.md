# Vocabulary study set (LINCS worksheets)

Ten words with their definitions, transcribed from the two LINCS worksheets.
Part of speech, synonyms, LINCing sentences and pictures were deliberately left
out — only the term and the definition are drilled.

| File | What it is |
| --- | --- |
| `vocab.json` | The ten terms and definitions. Source of truth for everything else. |
| `questions.py` | Builds the 10-question bank: each word asked for its meaning. |
| `build_kahoot_import.py` → `Kahoot-Vocab-Quiz-Import.xlsx` | Kahoot bulk-import spreadsheet. |
| `build_study_page.py` + `study_page_template.html` → `vocab-buzzer.html` | Playable quiz page. |

## Importing into Kahoot

1. Sign in at kahoot.com and choose **Create → Kahoot**.
2. Click **Import spreadsheet** and upload `Kahoot-Vocab-Quiz-Import.xlsx`.
3. Kahoot reads columns A–G starting at row 9; rows 1–8 are instructions and headers.

All ten questions read "What does X mean?" and offer four definitions — the correct one
plus three belonging to other words on the list.

## Rebuilding after editing the words

Edit `vocab.json`, adjust the distractors in `questions.py` if the word list
changes, then:

```sh
pip install openpyxl
python3 build_kahoot_import.py
python3 build_study_page.py
```

## The words

| Term | Definition |
| --- | --- |
| abstain | hold back |
| accede | to agree to a request or demand |
| accessible | obtainable |
| affluent | having a large amount of money or wealth |
| animate | to make something alive; give it energy and spirit |
| boisterous | loud, energetic |
| cordial | affectionate, politely warm |
| cunning | clever at being deceitful |
| curtail | reduce, restrict, shorten |
| diligent | showing care in doing one's work |
