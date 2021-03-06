/**
 * MIDI Show Control
 * MSC routines header
 *
 * This file contains the external defines and prototypes for interpreting
 * MSC commands.
 *
 * Last modified March 25, 2015
 *
 * Copyright (C) 2015. All Rights Reserved.
 */

#ifndef MSC_H
#define MSC_H

/******************************************************************************
 * Includes
 ******************************************************************************/

#include "Arduino.h"

/******************************************************************************
 * External constants
 ******************************************************************************/

/**
 *  Stores the endpoint type of an MSC message
 */
typedef enum TYPE_TAG {
  LIGHT = 0x01,
  SOUND = 0x10,
  FIREWORKS = 0x61,
  ALL = 0x7f
} TYPE;

/**
 *  Stores the command type of an MSC message
 */
typedef enum COMMAND_TAG {
  GO = 0x01,
  STOP,
  RESUME,
  TIMED_GO,
  LOAD,
  SET,
  FIRE,
  ALL_OFF,
  RESTORE,
  RESET,
  GO_OFF,
  STANDBY_PLUS = 0x11,
  STANDBY_MINUS,
  SEQUENCE_PLUS,
  SEQUENCE_MINUS
} COMMAND;

typedef struct ARRAY_TAG {
  const byte* data;
  int length;
} ARRAY;

#define SYSEX_END_BYTE          0xF7

#define MAX_CUE_LEN             8 //The maximum length of a cue string
#define MAX_LIST_LEN            8 //The maximum length of a list string

/******************************************************************************
 * Class definition
 ******************************************************************************/

class MSC {
public:
  MSC(const byte* packet, int len);

  byte getID();
  TYPE getType();
  COMMAND getCommand();
  char* getCue();
  char* getList();
  const byte* getData();
  int getLength();

private:
  byte id;
  TYPE type;
  COMMAND command;
  char cue[MAX_CUE_LEN + 1];
  char list[MAX_LIST_LEN + 1];
  const byte* data;
  int length;
};

#endif