// How to Survive 7th Grade — by Jacob Garcia
// Body text: Times New Roman | Titles & chapter headings: Arial
const {
  Document, Packer, Paragraph, TextRun, HeadingLevel,
  AlignmentType, PageBreak, Footer, PageNumber, LevelFormat
} = require("docx");
const fs = require("fs");

const TNR = "Times New Roman";
const ARIAL = "Arial";

// ---------- helpers ----------
// Body paragraph (Times New Roman, 12pt, first-line indent, novel spacing)
function p(text) {
  return new Paragraph({
    spacing: { after: 140, line: 300 },
    indent: { firstLine: 360 },
    children: [new TextRun({ text, font: TNR, size: 24 })],
  });
}

// Body paragraph with a bold lead-in (still Times New Roman) — used for the Survival Guide
function pLead(lead, rest) {
  return new Paragraph({
    spacing: { after: 140, line: 300 },
    children: [
      new TextRun({ text: lead, font: TNR, size: 24, bold: true }),
      new TextRun({ text: rest, font: TNR, size: 24 }),
    ],
  });
}

// A centered "scene break" of three little stars, Times New Roman
function starBreak() {
  return new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { before: 120, after: 160 },
    children: [new TextRun({ text: "*   *   *", font: TNR, size: 24 })],
  });
}

// Chapter opener: small Arial label + big Arial title, new page
function chapter(label, title) {
  return [
    new Paragraph({ children: [new PageBreak()] }),
    new Paragraph({
      alignment: AlignmentType.CENTER,
      spacing: { before: 400, after: 60 },
      children: [
        new TextRun({
          text: label.toUpperCase(),
          font: ARIAL, size: 22, bold: true, color: "555555",
          characterSpacing: 40,
        }),
      ],
    }),
    new Paragraph({
      heading: HeadingLevel.HEADING_1,
      alignment: AlignmentType.CENTER,
      spacing: { after: 320 },
      children: [
        new TextRun({ text: title, font: ARIAL, size: 34, bold: true, color: "1a1a1a" }),
      ],
    }),
  ];
}

// ---------- title page ----------
const titlePage = [
  new Paragraph({ spacing: { before: 1600 } }),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 120 },
    children: [new TextRun({ text: "HOW TO SURVIVE", font: ARIAL, size: 60, bold: true, color: "1a1a1a" })],
  }),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 260 },
    children: [new TextRun({ text: "7th GRADE", font: ARIAL, size: 96, bold: true, color: "1a1a1a" })],
  }),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 700 },
    children: [
      new TextRun({
        text: "A Totally True (Mostly) Field Guide to the Wild Jungle of Middle School",
        font: ARIAL, size: 26, italics: true, color: "555555",
      }),
    ],
  }),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    spacing: { after: 40 },
    children: [new TextRun({ text: "By", font: ARIAL, size: 28, color: "555555" })],
  }),
  new Paragraph({
    alignment: AlignmentType.CENTER,
    children: [new TextRun({ text: "Jacob Garcia", font: ARIAL, size: 44, bold: true, color: "1a1a1a" })],
  }),
];

// ---------- book content ----------
// Each entry: {label, title, paras: [ ... ]} where a para is a string,
// or "***" for a star break, or {lead, rest} for a bold lead-in line.
const book = [
  {
    label: "Prologue",
    title: "Read This First (Seriously)",
    paras: [
      "Okay. If you are reading this, one of two things is happening.",
      "One: you are about to start seventh grade, and you are terrified. Good. That means your brain still works. Keep reading.",
      "Two: you are a fully grown adult who picked up this book by accident, thinking it was a coupon for tacos. In that case, welcome. This is not a coupon for tacos. But it is more exciting than one, and nobody can take that away from us.",
      "My name is Milo Pipkin, and last year I survived seventh grade. Barely. I lost a shoe, a science project, most of my dignity, and one entire eyebrow. But I made it out alive, which basically makes me an expert. Think of me as your tour guide through the wild and dangerous jungle known as MIDDLE SCHOOL. I have seen things. I have smelled the cafeteria on Meatloaf Monday. I have survived.",
      "Here is the very first thing nobody tells you, and it is going to make you mad.",
      "The word school comes from an ancient Greek word, skhole, and back then it did not mean pop quizzes and gym shorts. It meant free time. It meant leisure. It meant sitting around relaxing.",
      "I am not making that up. Thousands of years ago, going to school meant taking it easy. Then somewhere along the way, some genius added homework, dodgeball, and a locker with a mind of its own, and here we are.",
      "So grab a snack. Lock your bedroom door so your little sister cannot get in. And whatever you do, do not lose this book, because it just might save your life.",
      "Or at least your locker.",
    ],
  },
  {
    label: "Chapter One",
    title: "The Locker That Ate My Arm",
    paras: [
      "The first day of seventh grade started with a betrayal, and the betrayer was made of metal.",
      "Her name was Locker 237.",
      "I did not choose Locker 237. Locker 237 chose me, the way a mosquito chooses the one ankle you cannot reach. It sat at the very end of the seventh grade hallway, dented in one corner, humming a low metal hum that said, very clearly, today is going to be the worst day of your life, small human.",
      "My combination was 12, 34, 8. I know this because I wrote it on my hand, on my shoe, and on a sticky note inside my backpack, because I trust myself about as far as I can throw a piano.",
      "I spun the dial. Right to 12. Left past 12 all the way around to 34. Right to 8. I pulled the little handle.",
      "Nothing.",
      "I tried again. Nothing. I tried a third time, faster, the way you press an elevator button eleven times like the elevator can feel your desperation.",
      "Nothing. Nothing. Nothing.",
      "That is when Charlie appeared, because Charlie always appears at the exact moment you least want a fun fact.",
      "Charlie Nguyen has been my best friend since second grade, when we bonded over both being sent to the principal for the same crime, which was laughing so hard that milk came out of our noses at the same time. Charlie is a walking encyclopedia. His brain is stuffed with facts the way a couch is stuffed with lost pennies, and they fall out of him at random.",
      "You know, Charlie said, watching me sweat, a lock like that has forty numbers, and you need three of them in the right order. So the number of possible combinations is forty times forty times forty. That is sixty-four thousand.",
      "I stared at him. Sixty-four thousand, I said.",
      "Sixty-four thousand, he agreed cheerfully. So really, getting it open at all is a mathematical miracle. Congratulations in advance.",
      "This was not the emotional support I needed. But it was, annoyingly, a real fact, and I filed it away in the tiny corner of my brain that was not currently panicking.",
      "***",
      "I spun the dial one more time, very carefully, whispering the numbers like a magic spell. Twelve. Thirty-four. Eight. I pulled.",
      "The locker popped open.",
      "I was so shocked that I did the natural thing, which was to shove my whole arm inside to grab my books, at the exact moment a semi-truck disguised as a human being slammed into my back.",
      "The semi-truck was named Brock Bumgarner. Brock is the size of a refrigerator and moves like a bowling ball with legs. He did not even notice me. He just rumbled past, laughing at something, and the force of it pushed my arm deeper into the locker, where my backpack strap snagged on the little hook, and suddenly I was stuck. Fully stuck. Arm swallowed by locker, cheek pressed against cold metal, breathing in the ancient smell of old gym socks and pencil shavings.",
      "And that, of course, is the exact moment she walked by.",
      "Zoe Alvarez. New this year. Curly hair, a backpack covered in enamel pins, and a way of looking at the world like it was slightly funnier than everyone else realized. She looked at me. She looked at the locker eating my arm. She tilted her head.",
      "Is it winning? she asked.",
      "My mouth opened. My brain, which had been working perfectly fine one second ago, produced exactly zero words. What came out instead was a sound. I cannot spell the sound. It was somewhere between a hiccup and a dying balloon.",
      "Zoe nodded slowly, like this was a completely reasonable answer, and walked on.",
      "It took Charlie four minutes to free my arm. The bell rang while my elbow was still trapped. I walked into first period nine minutes late, with a red line across my cheek shaped exactly like a locker vent, and I understood, deep in my bones, the first great truth of middle school.",
      "The locker can smell fear. And it is always hungry.",
    ],
  },
  {
    label: "Chapter Two",
    title: "Mystery Meat and Other Cafeteria Crimes",
    paras: [
      "There are places in this world that follow the normal rules of civilization, and then there is the seventh grade cafeteria.",
      "The cafeteria is not a room. It is a kingdom, with borders, and armies, and unwritten laws older than the school itself. Every table is a country. Sit at the wrong one and you might accidentally start a war you did not know existed.",
      "I learned this the hard way on day two, holding a tray and standing in the exact middle of the room like a lost tourist, spinning slowly in a circle while two hundred kids pretended not to watch me have a nervous breakdown.",
      "But before the tables, you have to survive the Line.",
      "The lunch line moves like a glacier and ends at a metal counter, where a woman named Miss Deb stands guard with an ice cream scoop and eyes that have seen everything. On day two, the special was something called Meatloaf Surprise.",
      "I do not know what the surprise was. I did not want to know. The number one rule of the cafeteria, which I invented right there on the spot, is this: never trust anything labeled Surprise. A surprise is good on your birthday. A surprise is bad on a lunch tray. Those are different kinds of surprise, and the cafeteria knows it.",
      "Charlie, who had already loaded his tray, leaned over as I poked the meatloaf with a spork. It poked back. I am almost sure it poked back.",
      "You know what never goes bad? Charlie said. Honey.",
      "What? I said.",
      "Honey, he said. It literally never spoils. Archaeologists found pots of honey in ancient Egyptian tombs, over three thousand years old, and it was still perfectly good to eat. Something about honey is so dry and acidic that bacteria cannot grow in it. Three thousand years, Milo. That meatloaf will not survive three thousand seconds.",
      "I looked down at the two little honey packets on my tray. They were, I realized, the only food item in that entire cafeteria I could actually trust. If honey could survive three thousand years in a pyramid, it could survive whatever the cafeteria was planning. I put them in my pocket like tiny golden treasures.",
      "***",
      "That is when the meatball happened.",
      "Across the cafeteria, Brock Bumgarner was demonstrating his hidden talent, which turned out to be launching food using a spoon like a catapult. A single meatball rose off his tray, arced across the entire room in beautiful, terrible slow motion, spun three times, and landed with a soft, wet, historic splat directly in the center of my forehead.",
      "The whole cafeteria gasped, then roared. Two hundred kids. All at once. Somewhere, a teacher yelled. Somewhere else, someone started a slow clap. I stood frozen, meatball on forehead, honey in pocket, and I felt the sauce begin to drip.",
      "It would come to be known, in the legends of our school, as the Great Meatball Incident. People would talk about it for months. Some of them still call me Meatball. I have made peace with this. Mostly.",
      "Zoe found me afterward at the napkin dispenser, scrubbing my forehead. She handed me an extra napkin without a word. Then she said, For what it is worth, that was a perfect shot. Physics loved that shot.",
      "And weirdly, that made me feel better. It is hard to explain. But when your worst moment gets called a perfect shot by the funniest person in seventh grade, it stops being your worst moment and starts being a story.",
      "I ate my honey packets. They were, as promised, delicious and ancient-proof. And I survived lunch.",
    ],
  },
  {
    label: "Chapter Three",
    title: "Dodgeball Is Basically Legal Warfare",
    paras: [
      "Gym class in seventh grade is where they take a group of nervous children and hand them foam weapons.",
      "Our gym teacher was Coach Ferraro, a man built like a fire hydrant and roughly as gentle. He wore a whistle the way a king wears a crown, and he had exactly one word for every problem in life. That word was hustle.",
      "My locker room outfit did not fit. The gym shirt they gave me was so large it could have been a parachute. The shorts came down past my knees. I looked less like an athlete and more like a kid who had been swallowed by a much bigger kid's laundry.",
      "Then Coach blew the whistle and said the two most terrifying words in the English language.",
      "Dodgeball. Today.",
      "My strategy for dodgeball is simple and has never once failed to embarrass me. Step one: find the biggest kid on my team. Step two: stand directly behind that kid and treat them like a human shield. Step three: pray.",
      "The problem is that my heart does not understand strategy. The second that whistle blew, my heart started slamming against my ribs like it was trying to escape and file a complaint.",
      "Charlie, who was standing next to me because Charlie also treats dodgeball as a survival situation, leaned in. Fun fact, he whispered. Your heart beats about a hundred thousand times a day. And it pumps around two thousand gallons of blood. Right now, yours is doing maybe five hundred beats a minute out of sheer terror, which is not medically possible, so congratulations, you are making history.",
      "Not helping, Charlie, I whispered back.",
      "Then Brock got the ball.",
      "***",
      "Here is a real fact that I did not learn until later, and I wish I had known it in that moment, because it would have explained a lot. The fastest animal on Earth is the peregrine falcon. When it dives, it can go over two hundred miles per hour. Faster than a race car. Faster than a roller coaster. The fastest living thing on the entire planet.",
      "I bring this up because when Brock Bumgarner throws a dodgeball, he throws it like a peregrine falcon that has a personal grudge against you.",
      "The ball left his hand. I did not see it travel. Physics does not work that fast for regular humans. One moment Brock was holding a ball, and the next moment there was a red circle stamped on my stomach and I was sitting on the gym floor wondering what year it was.",
      "But here is the part nobody believes.",
      "As I fell, the ball bounced up off me, spun through the air, and smacked Brock's own teammate right in the shoulder. That kid was out. And because I had caught the ball on the bounce off my own body, or something, Coach Ferraro pointed at me and yelled, Pipkin! Out but heroic! and the whole class lost their minds.",
      "I did not understand the rules. I still do not understand the rules. But I had somehow taken a hit and knocked out an enemy at the same time, purely by falling down, and for one shining moment, I was a dodgeball legend.",
      "The lesson of gym class is this. You cannot control the falcon. You can only control where you stand, who stands in front of you, and how you fall. And if you are going to get hit, get hit heroically.",
    ],
  },
  {
    label: "Chapter Four",
    title: "The Homework Avalanche",
    paras: [
      "By the third week of seventh grade, I understood that teachers had formed a secret club.",
      "The club has one rule. Every teacher must assign a giant pile of homework on the exact same night, and none of them are allowed to tell each other. That is the only explanation. There is no other reason that math, science, English, history, and Spanish would all decide, at the same time, on a Tuesday, to bury me alive.",
      "I came home with a backpack so heavy it left grooves in my shoulders. I dumped it on the kitchen table, and worksheets slid out like an avalanche, and I put my face down on the table and made the dying balloon sound again.",
      "That is when my little sister, Penny, who is six and made entirely of chaos and glitter, climbed onto the chair next to me, looked at my mountain of homework, and said, I will help.",
      "Twenty minutes later, my history worksheet had a drawing of a cat on it. My math homework said MILO SMELLS in orange marker. And one entire page of my Spanish assignment was simply gone, which Penny explained by saying, very calmly, The dog needed it.",
      "We do not have a dog.",
      "***",
      "My plan, that first big homework night, was to do it all at once, at midnight, by cramming. I would pour every fact into my brain in one giant, panicked, midnight session, like stuffing a suitcase by sitting on it.",
      "This did not work. It never works. And Charlie, the next morning, explained exactly why, in that cheerful way he has that makes you want to hug him and throw a shoe at him at the same time.",
      "Your brain is not a trash bag you cram at midnight, he said. It is more like a slow cooker. Here is the actual science. When you sleep, your brain sorts through everything you learned that day and files the important stuff into your long-term memory. That is called memory consolidation. If you cram all night and skip sleep, your brain never gets to do the filing. So you basically studied for nothing.",
      "He was just getting started.",
      "And there is more, Charlie said, following me down the hall. If you study a little bit every day for five days, instead of a giant chunk the night before, you remember way more. It is called spaced repetition. Little bits, spread out, beat one giant panic session every single time. Also, your brain is only about two percent of your body weight, but it uses about twenty percent of your energy. Twenty percent! For something the size of a grapefruit. So feed it, and let it sleep, or it will quit on you.",
      "I hated how much sense this made.",
      "So I tried it. I made a planner, a real one, not the crumpled one at the bottom of my backpack. I did a little homework each afternoon instead of all of it at midnight. I actually slept. And the wild, unbelievable thing is that it worked. My grades went up. My eye bags went down. And I discovered the fourth great truth of seventh grade.",
      "Your brain is a slow cooker, not a microwave. Give it a little time, and it makes something good. Blast it all at once, and you get a hot mess.",
    ],
  },
  {
    label: "Chapter Five",
    title: "Group Project of Doom",
    paras: [
      "There are four words that can turn a happy classroom silent with dread. Ms. Okonkwo said them on a Monday, smiling, like she was handing out candy.",
      "We will do groups.",
      "Group projects are the ultimate test of survival, because you cannot pick your group. Fate picks it for you. And fate, that day, had jokes.",
      "My group was me, Charlie, Zoe Alvarez, and, because the universe wanted to watch me suffer, Brock Bumgarner. Our topic, pulled from a hat, was the ocean.",
      "Now, I know what you are thinking. That is a nightmare team. And at first, it was. Every group project has the same cast of characters. There is the one who does everything, which was Zoe. There is the one who supplies random facts, which was Charlie. There is the one who tries hard but mostly makes the dying balloon sound, which was me. And there is the one who does absolutely nothing except eat the supplies, which was Brock, who ate three of our blue gummy fish before we even started, and then two of our green ones, and then, and I still cannot believe this, part of the glue stick.",
      "***",
      "But here is what happened when we actually started learning about the ocean. It turns out the ocean is bananas.",
      "Charlie kicked us off. The ocean covers about seventy-one percent of the whole planet, he said. Almost three quarters of Earth is water. The biggest ocean is the Pacific, and it is so huge that all the continents on Earth could fit inside it with room left over.",
      "Zoe found the best fact, though. Did you guys know, she said, eyes wide, that the biggest animal that has ever lived is alive right now? Not a dinosaur. The blue whale. It is bigger than any dinosaur that ever existed. Its heart is about the size of a small car. And its tongue can weigh as much as an elephant.",
      "We all sat there for a second, imagining a heart the size of a car.",
      "That is when Brock, mouth full of our gummy fish, said the smartest thing anyone said all week. So we should build one, he said. A big one. And make people walk under it.",
      "And here is the thing about Brock Bumgarner that I did not know until that exact moment. He is an absolute genius at building stuff. While the rest of us made the poster, Brock built a blue whale out of cardboard, chicken wire, and about nine rolls of tape, and it was enormous, and it hung from the ceiling, and when our class walked under it and looked up, everybody went, whoa.",
      "We got an A. Brock cried a little. He said it was allergies. It was not allergies.",
      "The lesson of the group project is one I did not expect. Everybody in your group is good at something, even the kid who ate the glue. Your only job is to figure out what that something is before the project is due.",
    ],
  },
  {
    label: "Chapter Six",
    title: "My Heart Did a Backflip (and Other Embarrassing Facts)",
    paras: [
      "Somewhere in the middle of building a cardboard blue whale, I developed a serious medical condition. The condition was that every time Zoe Alvarez talked to me, my face caught on fire.",
      "Not real fire. Face fire. The kind where your cheeks go so hot and so red that you could probably toast a marshmallow on them. It happened every single time. She would say something normal, like can you hand me the tape, and my whole head would light up like a stoplight, and my brain would leave the building, and I would hand her the scissors instead of the tape, or, one memorable time, my own shoe.",
      "I thought I was broken. I told Charlie I thought I was dying. Charlie, naturally, had science ready.",
      "You are not dying, he said. You are blushing. Here is exactly what is happening. When you get nervous or embarrassed, your body dumps out a chemical called adrenaline. Adrenaline makes the tiny blood vessels in your cheeks get wider, so more blood rushes to your face, and blood is red, so your face turns red. And the worst part, Milo? You cannot stop it. It is automatic. Your body does it without asking you.",
      "Oh good, I said. So there is no cure.",
      "There is no cure, Charlie said happily.",
      "***",
      "It gets worse. There was also the stomach thing. Every time I saw Zoe coming down the hall, my stomach would do this flip, this fluttery, ticklish, oh no here she comes feeling. Butterflies, people call it. I asked Charlie about that too, because at this point Charlie was basically my personal doctor.",
      "Same villain, he said. Adrenaline again. When you are nervous, your body decides it does not have time to digest food, so it slows your stomach way down and sends that energy to your arms and legs instead, in case you need to run away from a bear. There is no bear. It is just Zoe. But your body does not know the difference, so you get that fluttery feeling. Butterflies are basically your stomach panicking about a bear that is actually a person you like.",
      "So let me get this straight, I said. I cannot control my face, and I cannot control my stomach.",
      "Correct, said Charlie. Your body is running the whole show. You are just along for the ride.",
      "Here is the strange, good part, though. One day Zoe caught me blushing, full stoplight, and instead of laughing, she went a little pink herself. And she said, I do that too, you know. Turn red. My whole life. My grandma calls me Tomato.",
      "And just like that, the most embarrassing thing about me became the least scary thing in the world, because it turned out she had it too. That is the sixth truth of seventh grade, and it took me a whole year to really believe it.",
      "You cannot control your face. Science says so. So stop trying to hide it, because the person worth liking is probably blushing right back.",
    ],
  },
  {
    label: "Chapter Seven",
    title: "The Substitute From Outer Space",
    paras: [
      "One Thursday, Ms. Okonkwo was out sick, and in her place stood the strangest adult I have ever seen in a school building.",
      "His name, he said, was Mr. Zither. He wore socks with sandals, a tie with tiny planets on it, and the wide, delighted smile of a man who has been waiting his whole life for exactly this moment. He looked at all of us, spread his arms, and said, Greetings, Earthlings. Today, we go to space.",
      "Somebody in the back muttered, this guy is an alien, and honestly, the jury is still out.",
      "But here is the thing. It was the best class of the entire year, and I am not even embarrassed to say it.",
      "***",
      "Mr. Zither did not use the textbook. He turned off the lights, clicked on a projector full of stars, and just started blowing our minds.",
      "On Venus, he said, a day is longer than a year. Venus spins so incredibly slowly that by the time it finishes one single spin, it has already gone all the way around the Sun. Imagine a birthday that comes before the day is even over.",
      "He clicked to the next picture. Jupiter, he said. The biggest planet. So big that you could fit about thirteen hundred Earths inside it. Thirteen hundred! Pour thirteen hundred Earths into Jupiter and it would swallow every one.",
      "He clicked again. The Sun. When you look at sunlight, he said, you are looking at the past. The light leaving the Sun takes about eight minutes to reach your eyeballs. So the sunshine on your face right now actually left the Sun eight minutes ago. If the Sun blinked out this very second, we would not even know for eight whole minutes.",
      "The whole class was dead silent. Even Brock. Especially Brock.",
      "And here is my favorite thing about space, Mr. Zither said, lowering his voice like he was telling a secret. It is completely silent. In space, there is no air, and sound needs air to travel through. So a star could explode right next to you, the biggest explosion imaginable, and you would not hear a single thing. Space is the quietest place there is.",
      "Then he taught us a trick to remember the order of the planets from the Sun. My Very Educated Mother Just Served Us Nachos. Mercury, Venus, Earth, Mars, Jupiter, Saturn, Uranus, Neptune. I still know it. I will know it when I am ninety. Nachos saved me.",
      "When the bell rang, nobody moved. We all just sat there in the dark, staring at a picture of Saturn, feeling very small in the best possible way. Mr. Zither switched the lights back on and said, Class dismissed, Earthlings. Try not to explode.",
      "The lesson of the substitute is that they are a mystery box. Sometimes you open it and get pure chaos. And sometimes, just sometimes, you get a man in sandals who makes the whole universe feel like it is yours.",
    ],
  },
  {
    label: "Chapter Eight",
    title: "Picture Day: A Tragedy in One Photo",
    paras: [
      "Every year, the school hires a photographer to permanently capture the single worst version of your face, laminate it onto an ID card, and mail a copy to your grandma. This holy disaster is called Picture Day.",
      "My hair has a mind of its own. Specifically, there is one chunk at the back that sticks straight up no matter what I do, like an antenna picking up radio stations from space. My mom calls it a cowlick.",
      "Fun fact I learned that morning, from Charlie, obviously, by text, at seven in the morning: it is called a cowlick because it looks like a cow came along and licked your hair into that shape. Real cows really do this to each other, and the swirl of hair looks the same, so people named yours after it. So there is a cow, somewhere, responsible for my ruined photo. I would like to have a word with that cow.",
      "My mom tried everything. Water. Gel. A comb. A different comb. Her own spit, which, no. She pressed that chunk of hair down flat, held it, counted to ten, let go, and it sprang straight back up like a spring-loaded trap. We fought that cowlick for forty-five minutes. The cowlick won. The cowlick always wins.",
      "***",
      "Here is another fact, and this one is actually beautiful. The word photograph comes from two old Greek words that mean drawing with light. Photo means light, and graph means drawing. So a photograph is literally a picture drawn by light itself. When you take a photo, light bounces off your face, goes into the camera, and draws you.",
      "Which would be lovely, if the light had not chosen that exact moment to draw me sneezing.",
      "It happened in slow motion. I sat on the little stool. The photographer said, Big smile! I smiled the biggest, fakest, most desperate smile of my life. And right then, right at the click, a tickle rose in my nose, and I sneezed so hard that my eyes slammed shut, my cowlick shot up, and my mouth opened into a shape that can only be described as a screaming door.",
      "The photographer looked at his screen. He did not say anything. He just slowly turned it around so I could see. It was, without question, the worst photograph ever drawn by light in the entire history of the human race.",
      "Charlie tried to make me feel better with one more fact. You know, he said, no two people in the whole world have the exact same fingerprints. Not even identical twins. There is only one you. And now, thanks to the light, there is only one photo like this one, too.",
      "Retake Day is in two weeks. I have already begun training my cowlick. We have reached an understanding. The understanding is that it will do whatever it wants and I will simply have to live with it, because it is the only one of me there is.",
    ],
  },
  {
    label: "Chapter Nine",
    title: "The Science Fair Explosion",
    paras: [
      "For the science fair, Charlie and I decided to build a volcano. This was not original. Half the gym was volcanoes. But we were not going to make a normal volcano. We were going to make the biggest, most powerful volcano the science fair had ever seen.",
      "This was our first mistake, and it was a beautiful one.",
      "The science behind a baking soda volcano is real and simple, and I can explain it now because I understand it very deeply, having worn it. When you mix baking soda with vinegar, they react and create a gas called carbon dioxide. That gas needs somewhere to go, so it bubbles up fast and rushes out as foam. That foam is the eruption. It is not lava. It is just gas in a huge hurry.",
      "So obviously, Charlie and I reasoned, if a little baking soda makes a little eruption, then a truly enormous amount of baking soda would make a truly enormous eruption. This is called science. This is also called a terrible idea, but we did not know that yet.",
      "***",
      "While we built, Charlie fed me volcano facts, because he cannot help it, it is a reflex.",
      "The biggest volcano in the whole solar system is not even on Earth, he said. It is on Mars. It is called Olympus Mons, and it is so tall it is almost three times the height of Mount Everest. If you stood at the bottom, you could not even see the top. It would disappear into the sky.",
      "That is when Mr. Okafor, the science teacher, walked over to inspect our setup. He looked at our volcano. He looked at the family-size box of baking soda. He looked at the gallon jug of vinegar. He opened his mouth to say something wise and cautionary.",
      "He was too late.",
      "Charlie had already poured. And I had already poured. And for one single, silent second, nothing happened, and we leaned in close to see why.",
      "Then the volcano erupted like it had been holding a grudge since the beginning of time.",
      "A tower of red foam blasted straight up, hit the gym ceiling, and rained back down on everything within ten feet, which included me, Charlie, Mr. Okafor, a first-place ribbon that was not even ours, and Brock Bumgarner, who happened to be walking past and got a direct hit to the back of the head. He turned around, dripping, and I braced to be flattened.",
      "Instead, Brock started laughing. Big, roaring, whole-body laughing. Do it again, he said.",
      "We did not win the science fair. But Mr. Okafor gave us a special ribbon he made on the spot with a marker. It said Most Dramatic, and he said we had demonstrated the principle of carbon dioxide gas more thoroughly than any student in the history of the school, mostly by wearing it. I keep that ribbon on my wall. It is my favorite thing I have ever earned.",
      "The lesson of the science fair is that more is not always better. But I will be honest with you. More was extremely more fun.",
    ],
  },
  {
    label: "Chapter Ten",
    title: "When Best Friends Go to War",
    paras: [
      "I have to tell you about the worst thing I did in seventh grade. I am not proud of it. But a survival guide that only tells you the wins is not a real survival guide.",
      "Here is what happened. Charlie and his facts had always been my favorite thing. But one day, standing by the lockers with Zoe and a bunch of other kids, Charlie started in on a fact about earthworms, and everybody kind of looked at each other, and I felt my face go hot, and instead of laughing like I always did, I said something mean.",
      "I said, Charlie, does anyone actually care about your random facts?",
      "The hallway went quiet. Charlie's face did a thing I had never seen it do. It closed, like a door. Oh, he said. Sorry. And he walked away, and he did not spout a single fact for the rest of the day, and the quiet where his facts used to be was the loudest thing I have ever heard.",
      "***",
      "We did not talk for a week. A whole week. I told myself I did not care. I cared so much I could not sleep.",
      "It turns out I could not even escape it in science class, because we were studying animals, and every animal reminded me of what a bad friend I had been.",
      "Did you know, Mr. Okafor said, that dolphins have names for each other? Each dolphin invents its own special whistle, a signature whistle, and the other dolphins learn it, and they call to each other across the ocean using those whistles. They are literally saying each other's names.",
      "I thought about Charlie whistling a fact across the ocean and me pretending I could not hear it.",
      "And then Mr. Okafor said the one that got me. Even cows, he said, have best friends. Studies show that cows pick a special friend in the herd, and when you separate them from that friend, their heart rate goes up and they get stressed. When they are back with their best friend, they calm right down. Cows understand friendship better than a lot of people do.",
      "I sat there, and I thought, a cow would never do what I did. A cow would be a better friend than me. And when a cow is beating you at being a decent person, it is time to fix your life.",
      "***",
      "So I found Charlie at his locker after school, and my heart was doing five hundred panic beats a minute, and I said the truest thing I could think of. Charlie, I said, I was wrong, and I was mean, and your facts are my favorite thing about you, and I am sorry. Also, did you know cows have best friends and get stressed when they are apart? Because I looked it up. Because I missed you.",
      "Charlie was quiet for a second. Then he smiled, that huge Charlie smile, and said, Actually, they do. And their heart rate really does go up. You got the fact exactly right.",
      "And that was it. We were okay. Better than okay.",
      "The tenth truth of seventh grade is the most important one in this whole book. A real friend is worth more than looking cool for five seconds in a hallway. Do not trade a Charlie for a moment. Even the cows know that.",
    ],
  },
  {
    label: "Chapter Eleven",
    title: "The Big Test (Dun Dun Dunnn)",
    paras: [
      "At the end of the year came the Big Test. Math. Ms. Grimsley's math. The final boss.",
      "Ms. Grimsley is the toughest teacher in the school. She does not smile. She does not blink, as far as anyone can tell. She once handed back a test so covered in red pen that a kid asked if it was bleeding. She said, It is disappointed. That is different.",
      "The whole seventh grade was terrified. But here is the thing. By the time the Big Test came, I had accidentally become good at surviving. I had learned things. And now I was going to use every single trick I had picked up all year.",
      "***",
      "First, I remembered Charlie's slow cooker rule. I did not cram. I studied a little bit every day for a week, spaced out, letting my brain file everything away each night while I slept. Little bits, spread out. By test day, the math was not a stranger. It had moved in and unpacked.",
      "Second, I used the memory palace. This is a real trick that memory champions actually use to remember insane amounts of stuff. You picture a place you know really well, like your house, and you leave each thing you want to remember in a different room, like little furniture in your mind. Then when you need it, you just take a walk through your house and pick it back up. I put my math formulas all over my bedroom in my imagination. The order of operations was sitting on my bed. It worked shockingly well.",
      "Speaking of the order of operations, that was on the test, and I had a way to remember it: PEMDAS. Parentheses, Exponents, Multiplication and Division, Addition and Subtraction, in that order. Some people remember it as Please Excuse My Dear Aunt Sally. My dear aunt Sally is very good at math. I do not have an aunt Sally. I borrowed one for the test.",
      "And then there was my secret weapon: the nine trick. If you ever forget your nines, hold up all ten fingers. To do nine times four, bend down your fourth finger from the left. Now count the fingers still standing. Three fingers on the left of the bent one, six on the right. Three and six. Thirty-six. Nine times four is thirty-six. It works for every single nine, all the way up. And here is the bonus fact that saved me twice: in the whole nine times table, the two digits of the answer always add up to nine. Nine times three is twenty-seven, and two plus seven is nine. Nine times six is fifty-four, and five plus four is nine. Every time. If your answer to a nine problem does not add up to nine, you made a mistake. It is like the nines are checking your work for you.",
      "***",
      "I finished the Big Test with time to spare. When Ms. Grimsley handed it back three days later, there was almost no red pen on it. Almost none. At the top, in her sharp handwriting, it said one word.",
      "Better.",
      "From Ms. Grimsley, better is basically a parade. I nearly cried. It was allergies. It was not allergies.",
      "The lesson of the Big Test is that you are not the same kid who got eaten by a locker back in September. You have been quietly getting stronger this whole time without even noticing. When the boss shows up, you are more ready than you think.",
    ],
  },
  {
    label: "Chapter Twelve",
    title: "How to Actually Survive 7th Grade",
    paras: [
      "On the last day of seventh grade, I stood at Locker 237 and realized I was going to miss her.",
      "I know. I know. She ate my arm. She smelled like ancient gym socks. She humiliated me on the first day. But we had been through a lot together, that locker and me, and now I knew her combination by heart and could open her in one smooth spin, and somehow that felt like the whole year in one small motion. I started the year unable to open a locker. I ended it opening it without even looking.",
      "That is what surviving actually looks like. Not fireworks. Just a kid who used to struggle with something, doing it now without thinking about it.",
      "***",
      "I thought about all of it. The Great Meatball Incident. The honey packets that could survive three thousand years in a pyramid. Getting knocked down in dodgeball and somehow becoming a legend. Building a blue whale with a heart the size of a car. Blushing like a stoplight and finding out the person I liked blushed too. A substitute in sandals showing me that sunlight is eight minutes old. The worst photograph ever drawn by light. A volcano that erupted onto the entire gym. Losing Charlie for a week and learning that even cows know better than to throw away a best friend. And a math test with the word better written at the top.",
      "None of it went the way I planned. All of it turned into a story. And every single story taught me something real, whether I wanted to learn it or not.",
      "***",
      "Here is the last fact in this book, and it is my favorite, because it means the future is not decided yet.",
      "Your brain never stops changing. Every time you learn something new, actual physical connections form inside your head, little bridges between brain cells, and your brain literally rewires itself to fit the new thing. Scientists call this neuroplasticity. And it does not stop when you grow up. Your brain can build new bridges your entire life. Which means you can always, always get better at things. Even the things that ate your arm in September.",
      "So that is how you survive seventh grade. Not by being the biggest, or the coolest, or the one who never trips in the cafeteria. You survive by having a Charlie. By laughing when the meatball lands. By feeding your slow-cooker brain and letting it sleep. By finding out what everyone in your group is secretly good at. And by remembering that you are getting stronger the whole time, even on the days it feels like the locker is winning.",
      "As for me, I am going into eighth grade now, which is a whole new jungle, and I am a little terrified, which means my brain still works.",
      "But I have my honey packets. I have my nachos mnemonic. I have Charlie. And now, so do you, because I wrote it all down.",
      "Good luck out there, seventh grader. Watch out for the meatballs. And whatever you do, do not lose this book.",
    ],
  },
  {
    label: "Bonus",
    title: "Milo's Official 7th Grade Survival Guide",
    survival: true,
    paras: [
      "Tear this page out. Tape it inside your locker. It is everything I know, in one place.",
      { lead: "Rule 1. ", rest: "The locker can smell fear, and it is always hungry. Learn your combination cold. One day you will open it without even looking, and that is what winning feels like." },
      { lead: "Rule 2. ", rest: "Never trust anything labeled Surprise. When in doubt, take the honey. It survived three thousand years in a pyramid; it can survive lunch." },
      { lead: "Rule 3. ", rest: "You cannot control the falcon. You can only choose where you stand and how you fall. If you are going down, go down heroically." },
      { lead: "Rule 4. ", rest: "Your brain is a slow cooker, not a microwave. Study a little each day, and sleep, so your brain can file it all away. Cramming at midnight is studying for nothing." },
      { lead: "Rule 5. ", rest: "Everyone in your group is good at something, even the kid eating the glue. Your job is to find out what before the project is due." },
      { lead: "Rule 6. ", rest: "You cannot control your face. Blushing is automatic, and science says stop fighting it. The person worth liking is probably blushing right back." },
      { lead: "Rule 7. ", rest: "A substitute is a mystery box. Some days it is chaos. Some days it is the best class of the year. Stay open to the sandals guy." },
      { lead: "Rule 8. ", rest: "On Picture Day, your hair will betray you. Make peace with your cowlick. There is only one you, so even the bad photo is one of a kind." },
      { lead: "Rule 9. ", rest: "More is not always better. It is, however, usually more fun. Choose your explosions wisely, and stand back." },
      { lead: "Rule 10. ", rest: "A real friend is worth more than looking cool for five seconds in a hallway. Even cows know this. Do not trade a Charlie for a moment." },
      { lead: "Rule 11. ", rest: "When the boss test shows up, you are more ready than you think. You have been getting stronger all year without noticing." },
      { lead: "Rule 12. ", rest: "Your brain can build new bridges your whole life. You can always get better. So can everybody else. Be patient with both." },
      "That is it. That is the whole guide. Now go survive.",
      "— Milo Pipkin, official seventh grade survivor",
    ],
  },
];

// ---------- assemble ----------
const children = [...titlePage];

for (const ch of book) {
  children.push(...chapter(ch.label, ch.title));
  for (const para of ch.paras) {
    if (para === "***") {
      children.push(starBreak());
    } else if (typeof para === "object" && para.lead) {
      children.push(pLead(para.lead, para.rest));
    } else {
      children.push(p(para));
    }
  }
}

const doc = new Document({
  creator: "Jacob Garcia",
  title: "How to Survive 7th Grade",
  description: "A funny middle-school survival novel with real facts woven in.",
  styles: {
    default: {
      document: { run: { font: TNR, size: 24 } },
    },
  },
  sections: [
    {
      properties: {
        page: {
          size: { width: 12240, height: 15840 }, // US Letter
          margin: { top: 1440, bottom: 1440, left: 1440, right: 1440 },
        },
      },
      footers: {
        default: new Footer({
          children: [
            new Paragraph({
              alignment: AlignmentType.CENTER,
              children: [
                new TextRun({
                  children: [PageNumber.CURRENT],
                  font: TNR,
                  size: 20,
                  color: "888888",
                }),
              ],
            }),
          ],
        }),
      },
      children,
    },
  ],
});

Packer.toBuffer(doc).then((buf) => {
  const out = "How_to_Survive_7th_Grade.docx";
  fs.writeFileSync(out, buf);
  console.log("Wrote", out, "-", buf.length, "bytes");
});
