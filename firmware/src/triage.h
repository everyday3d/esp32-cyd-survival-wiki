// Triage assistant: a few menu questions that lead to the right guide. Built in, so it works
// with any SD card. Targets starting with "g:" are guide ids from /wiki/catalog.tsv; anything
// else is the id of another node. Keep labels short (they have to fit a button on the 2.8").
#pragma once

struct TriOpt  { const char *label; const char *to; };
struct TriNode { const char *id; const char *q; uint8_t n; TriOpt o[5]; };

static const TriNode TRIAGE[] = {
  {"root", "What is happening? Call emergency services first if you can.", 5, {
    {"Someone is hurt or sick", "person"},
    {"Storm, quake or disaster", "disaster"},
    {"Trouble at home or in a car", "home"},
    {"Lost, cold or need supplies", "outdoors"},
    {"A child, baby or older adult", "family"}}},

  {"person", "Are they awake and breathing normally?", 5, {
    {"No response, not breathing", "g:cpr"},
    {"Choking, cannot speak", "g:choking"},
    {"Heavy bleeding", "g:severe-bleeding"},
    {"Awake but hurt or ill", "hurt"},
    {"Poison, bite or allergy", "poison"}}},

  {"hurt", "What kind of problem is it?", 5, {
    {"Fall, crash or broken bone", "fall"},
    {"Chest pain, stroke, seizure", "heart"},
    {"Burn or electric shock", "burn"},
    {"Too hot or too cold", "temp"},
    {"Sick: vomiting, diarrhea", "g:diarrhea-vomiting"}}},

  {"fall", "After a fall, crash or blow:", 5, {
    {"Neck or back pain, numb", "g:spinal-injury"},
    {"Head injury", "g:head-injury"},
    {"Broken bone or sprain", "g:fractures-sprains"},
    {"Cut or wound", "g:wound-care"},
    {"Pulled from water", "g:drowning"}}},

  {"heart", "Which fits best?", 5, {
    {"Chest pain or stroke signs", "g:stroke-heart-attack"},
    {"Seizure", "g:seizures"},
    {"Diabetic, shaky, confused", "g:diabetic-emergency"},
    {"Pale, clammy, faint", "g:shock"},
    {"Eye injury", "g:eye-injuries"}}},

  {"burn", "What happened?", 4, {
    {"Burn", "g:burns"},
    {"Electric shock", "g:electric-shock"},
    {"Lightning or power line", "g:downed-power-lines"},
    {"Fire in the house", "g:house-fire"}}},

  {"temp", "Too hot or too cold?", 4, {
    {"Cold, shivering, frostbite", "g:hypothermia"},
    {"Hot, confused, not sweating", "g:heat-illness"},
    {"Fell through ice", "g:falling-through-ice"},
    {"Thirsty, dehydrated", "g:dehydration"}}},

  {"poison", "What was it?", 5, {
    {"Swallowed poison or drugs", "g:poisoning"},
    {"Severe allergic reaction", "g:anaphylaxis"},
    {"Snakebite", "g:snakebite"},
    {"Tick, sting or spider", "g:bites-stings"},
    {"Carbon monoxide", "g:carbon-monoxide"}}},

  {"disaster", "Which one?", 5, {
    {"Tornado", "g:tornado"},
    {"Flood", "g:flood"},
    {"Earthquake", "g:earthquake"},
    {"Wildfire", "g:wildfire"},
    {"More...", "disaster2"}}},

  {"disaster2", "Which one?", 5, {
    {"Hurricane", "g:hurricane"},
    {"Blizzard or ice storm", "g:blizzard"},
    {"Heat wave", "g:extreme-heat"},
    {"Nuclear emergency", "g:nuclear-get-inside"},
    {"Power outage", "g:power-outage"}}},

  {"home", "Where is the trouble?", 4, {
    {"Gas, fire or pipes", "utility"},
    {"Car broke down or stuck", "car"},
    {"Power lines or generator", "power"},
    {"Carbon monoxide alarm", "g:carbon-monoxide"}}},

  {"utility", "What is it?", 5, {
    {"Smell of gas", "g:gas-leak"},
    {"Fire in the house", "g:house-fire"},
    {"Burst or frozen pipe", "g:burst-pipe"},
    {"Shut off utilities", "g:utility-shutoffs"},
    {"Food spoiling", "g:food-after-outage"}}},

  {"power", "What is it?", 3, {
    {"Downed power line", "g:downed-power-lines"},
    {"Using a generator", "g:generator-safety"},
    {"Power is out", "g:power-outage"}}},

  {"car", "What is the car doing?", 5, {
    {"Broken down", "g:car-breakdown"},
    {"Stuck in the cold", "g:stranded-winter"},
    {"In water", "g:car-in-water"},
    {"On fire", "g:car-fire"},
    {"Flat tire", "g:flat-tire"}}},

  {"outdoors", "What do you need?", 5, {
    {"I am lost", "g:staying-found"},
    {"Water", "g:finding-water"},
    {"Shelter, warm and dry", "g:shelter-basics"},
    {"Fire", "g:fire-basics"},
    {"Signal for help", "g:sos-signals"}}},

  {"family", "Who needs help?", 5, {
    {"Lost or missing child", "g:lost-child"},
    {"Child with a fever", "g:child-fever"},
    {"Baby, no power", "g:baby-care"},
    {"Child hurt or ill", "g:children-first-aid"},
    {"Older adult", "g:elders-special-needs"}}},
};
static const int N_TRIAGE = sizeof(TRIAGE) / sizeof(TRIAGE[0]);
