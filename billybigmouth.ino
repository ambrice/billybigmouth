#include <Audio.h>
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <SerialFlash.h>

#define MOUTH_PIN_A 28
#define MOUTH_PIN_B 29

#define BODY_PIN_A 24
#define BODY_PIN_B 25

AudioPlaySdWav           playWav1;
AudioAmplifier           amp1;
AudioOutputI2S           audioOutput;
AudioConnection          patchCord1(playWav1, 0, amp1, 0);
AudioConnection          patchCord2(amp1, 0, audioOutput, 0);
AudioControlSGTL5000     sgtl5000_1;

/* Motor functions */

enum Movement {
  HEAD,
  MOUTH,
  TAIL,
  RELEASE,
};

struct MovementInstruction {
  Movement type;
  uint32_t timestamp;
  MovementInstruction *next;
};

void openMouth(uint16_t t) {
  digitalWrite(MOUTH_PIN_A, LOW);
  digitalWrite(MOUTH_PIN_B, HIGH);
  delay(t);
  digitalWrite(MOUTH_PIN_B, LOW);
}

void closeMouth() {
  digitalWrite(MOUTH_PIN_A, LOW);
  digitalWrite(MOUTH_PIN_B, LOW);
}

void moveHead() {
  digitalWrite(BODY_PIN_A, HIGH);
  digitalWrite(BODY_PIN_B, LOW);
}

void moveTail(uint16_t t) {
  digitalWrite(BODY_PIN_A, LOW);
  digitalWrite(BODY_PIN_B, HIGH);
  delay(t);
  digitalWrite(BODY_PIN_A, LOW);
  digitalWrite(BODY_PIN_B, LOW);
}

void releaseBody() {
  digitalWrite(BODY_PIN_B, LOW);
  digitalWrite(BODY_PIN_A, LOW);
}

/* Audio functions */

/* hard-code support for up to 10 files for now (ever) */
#define MAX_FILES 10
static char *wavfiles[MAX_FILES];
static MovementInstruction *instructions[MAX_FILES];

/* Read instructions from .dat file.  Instructions are like 'M030500'
 * to open the mouth 30.5 seconds into the song */
void readInstructions(const char *filename, MovementInstruction *inst)
{
  File datfile = SD.open(filename);
  if (!datfile) {
    Serial.printf("Instruction file %s not found\n", filename);
    return;
  }
  
  char t, timestampText[7];
  size_t n;
  MovementInstruction *prev = NULL;
  while (true) {
    n = datfile.read(&t, 1);
    if (n < 1) {
      break;
    }
    n = datfile.read(timestampText, 6);
    if (n < 6) {
      break;
    }
    timestampText[6] = '\0';
    if (t == 'M' || t == 'm') {
      inst->type = MOUTH;
    } else if (t == 'T' || t == 't') {
      inst->type = TAIL;
    } else if (t == 'H' || t == 'h') {
      inst->type = HEAD;
    } else if (t == 'R' || t == 'r') {
      inst->type = RELEASE;
    }
    inst->timestamp = (uint32_t)atol(timestampText);
    inst->next = new MovementInstruction();
    prev = inst;
    inst = inst->next;
    // Read until end of line '\n'.  Next characters should be \n or \r\n..
    n = datfile.read(&t, 1);
    while (n == 1 && t != '\n') {
      n = datfile.read(&t, 1);
    }
    if (n < 1) {
      break;
    }
  }

  if (prev) {
    delete prev->next;
    prev->next = NULL;
  }
}

/* Get the .wav and corresponding .dat files from sdcard */
void getFiles() {
  int idx=0;
  File dir = SD.open("/");
  File entry = dir.openNextFile();
  while (entry && idx < MAX_FILES) {
    if (entry.isDirectory()) {
      entry = dir.openNextFile();
      continue;
    } else {
      Serial.printf("Found file %s\n", entry.name());
    }
    size_t namelen = strlen(entry.name());
    if (strncmp(entry.name() + namelen - 4, ".WAV", 4) == 0 ||
        strncmp(entry.name() + namelen - 4, ".wav", 4) == 0) 
    {
      wavfiles[idx] = (char *)malloc(namelen+1);
      strncpy(wavfiles[idx], entry.name(), namelen+1);
      /* Find the .dat file with the instructions */
      char *datfile = (char *)malloc(namelen + 1);
      memset(datfile, 0, namelen + 1);
      strncpy(datfile, wavfiles[idx], namelen - 4);
      strcat(datfile, ".dat");
      instructions[idx] = (MovementInstruction *)malloc(sizeof(MovementInstruction));
      readInstructions(datfile, instructions[idx]);
      free(datfile);
      ++idx;
    }
    entry = dir.openNextFile();
  }
}

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  // GPIO setup
  pinMode(MOUTH_PIN_A, OUTPUT);
  pinMode(MOUTH_PIN_B, OUTPUT);
  pinMode(BODY_PIN_A, OUTPUT);
  pinMode(BODY_PIN_B, OUTPUT);
  
  // Audio connections require memory to work.  For more
  // detailed information, see the MemoryAndCpuUsage example
  AudioMemory(8);

  // Comment these out if not using the audio adaptor board.
  // This may wait forever if the SDA & SCL pins lack
  // pullup resistors
  sgtl5000_1.enable();
  amp1.gain(0.1);

  if (!(SD.begin(BUILTIN_SDCARD))) {
    // stop here, but print a message repetitively
    while (1) {
      Serial.println("Unable to access the SD card");
      delay(500);
    }
  }

  getFiles();
}

static int current = 0;

void loop() {
  // put your main code here, to run repeatedly:
  if (wavfiles[current] == NULL || current == MAX_FILES) {
    current = 0;
  }

  Serial.printf("Playing file %s\n",wavfiles[current]);
  playWav1.play(wavfiles[current]);

  // A brief delay for the library to read wav info
  delay(25);

  MovementInstruction *inst = instructions[current];
  inst = instructions[current];
  uint32_t pos = 0;
  while (playWav1.isPlaying()) {
    pos = playWav1.positionMillis();
    if (inst != NULL && pos >= inst->timestamp) {
      switch(inst->type) {
      case HEAD:
        moveHead();
        break;
      case TAIL:
        moveTail(100);
        pos += 100;
        break;
      case MOUTH:
        openMouth(100);
        pos += 100;
        break;
      case RELEASE:
        releaseBody();
        break;
      }
      inst = inst->next;
    }

    if (inst != NULL && inst->timestamp > pos && (inst->timestamp - pos) > 10) {
      delay(inst->timestamp - pos - 10);
    } else {
      delay(1);
    }
  }

  ++current;
  delay(600000);
}
