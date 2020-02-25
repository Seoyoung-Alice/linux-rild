/****************************************************************************
  sms.c
  Author : kpkang@gmail.com
*****************************************************************************/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h> 

#include "sms.h"
#include "m2mdbg.h"
#include "utils.h"


const unsigned char alphabet_table[] = 
{
  0x40, // @      COMMERCIAL AT
  0xA3, // ��     POUND SIGN
  0x24, // $      DOLLAR SIGN
  0xA5, // ��     YEN SIGN
  0xE8, // e      LATIN SMALL LETTER E WITH GRAVE
  0xE9, // e      LATIN SMALL LETTER E WITH ACUTE
  0xF9, // u      LATIN SMALL LETTER U WITH GRAVE
  0xEC, // i      LATIN SMALL LETTER I WITH GRAVE
  0xF2, // o      LATIN SMALL LETTER O WITH GRAVE
  0xC7, // C      ATIN CAPITAL LETTER C WITH CEDILLA
  0x0A, //        LINE FEED
  0xD8, // ��     LATIN CAPITAL LETTER O WITH STROKE
  0xF8, // ��     LATIN SMALL LETTER O WITH STROKE
  0x0D, //        CARRIAGE RETURN
  0xC5, // A      LATIN CAPITAL LETTER A WITH EVENT_RING ABOVE
  0xE5, // a      LATIN SMALL LETTER A WITH EVENT_RING ABOVE
  0xFF, // ��     GREEK CAPITAL LETTER DELTA
  0x5F, // _      LOW LINE
  0xFF, // ��     GREEK CAPITAL LETTER PHI
  0xFF, // ��     GREEK CAPITAL LETTER GAMMA
  0xFF, // ��     GREEK CAPITAL LETTER LAMBDA
  0xFF, // ��     GREEK CAPITAL LETTER OMEGA
  0xFF, // ��     GREEK CAPITAL LETTER PI
  0xFF, // ��     GREEK CAPITAL LETTER PSI
  0xFF, // ��     GREEK CAPITAL LETTER SIGMA
  0xFF, // ��     GREEK CAPITAL LETTER THETA
  0xFF, // ��     GREEK CAPITAL LETTER XI
  0x1B, //        ESCAPE TO EXTENSION TABLE
  0xC6, // ��     LATIN CAPITAL LETTER AE
  0xE6, // ��     LATIN SMALL LETTER AE
  0xDF, // ��     LATIN SMALL LETTER SHARP S (German)
  0xC9, // E      LATIN CAPITAL LETTER E WITH ACUTE
};


const unsigned char extension_table[][2] =
{
  { /*0x1B*/0x0A, 0x0C },  //      FORM FEED
 #if 0 
  { /*0x1B*/0x65, 0x20 },  // €     EURO SIGN
#endif
  { /*0x1B*/0x3C, 0x5B },  // [    LEFT SQUARE BRACKET
  { /*0x1B*/0x2F, 0x5C },  // \    REVERSE SOLIDUS (BACKSLASH)
  { /*0x1B*/0x3E, 0x5D },  // ]    RIGHT SQUARE BRACKET
  { /*0x1B*/0x14, 0x5E },  // ^    CIRCUMFLEX ACCENT
  
  { /*0x1B*/0x28, 0x7B },  // {    LEFT CURLY BRACKET
  {/*0x1B*/0x40, 0x7C },  // |    VERTICAL BAR
  { /*0x1B*/0x29, 0x7D },  // }    RIGHT CURLY BRACKET
  { /*0x1B*/0x3D, 0x7E },  // ~    TILDE
  
};

const int alphabet_table_size  = sizeof(alphabet_table)  / sizeof(alphabet_table[0]);
const int extension_table_size = sizeof(extension_table) / sizeof(extension_table[0]);

#define MAX_SMS_BYTES 140

/*
  TP-MTI [ Bit1 Bit0 (01) ] : SMS SUBMIT (in the direction MS to SC) 
  TP-VPF [ Bit4 Bit3 (10) ] : TP VP field present - relative format 
  TP-UDHI [ Bit6 (1) ] : TP UD field contains a Header
*/
#define TP_MTI 0x01
#define TP_VPF 0x10
#define TP_UDHI 0x40

/*
  type of address
  1 | Type-of-number | Numbering-plan-identification
*/
#define TON_Unknown 0
#define TON_International 0x1
#define TON_National 0x2

#define NPI_ISDN_telephone 0x1

int encode_alphabet(char *message, char *user_data)
{
  unsigned char ch;
  int user_data_length = 0;
  int count;

  
  while(0 != *message)
  {
    ch = (unsigned char)*message;
 
    for(count = 0; count < extension_table_size; count++)
    {
      if(extension_table[count][1] == ch)
      {
        ch = extension_table[count][0];
        break;
      }
    }

    if(extension_table_size == count)
    {
      for(count = 0; count < alphabet_table_size; count++)
      {
        if(alphabet_table[count] == ch)
        {
          ch = count;
          break;
        }
      }
    }
    else
    {
      user_data[user_data_length] = 0x1B;
      user_data_length++;
    }

    user_data[user_data_length] = ch;
    user_data_length++;

    message += 1;
  }

  return user_data_length;
}


int decode_alphabet(char *user_data, int user_data_length, char* message)
{
  unsigned char ch;
  int count;
  int index = 0;
  int extension;

  for(count = 0; count < user_data_length; count++, index++)
  {
    ch = (unsigned char)user_data[count];

    if(0x20 > ch)
    {
      if(0x1B == ch)
      {
        count++;

        ch = user_data[count];

        for(extension = 0; extension < extension_table_size; extension++)
        {
          if(extension_table[extension][0] == ch)
          {
            ch = extension_table[extension][1];
            break;
          }
        }

        if(extension_table_size == extension)
        {
          ch = 0x20;
        }
      }
      else
      {
        if (0xFF == alphabet_table[ch])
          ch = 0x20;
        else
          ch = alphabet_table[ch];
      }
    }

    message[index] = ch;
  }

  user_data_length = index;

  //if((1 < user_data_length) && ('@' == message[user_data_length - 1]))
  //{
  //  user_data_length--;
  //}

  return user_data_length;
}

/*
  inSMSTxt: SMS Message
  inType : MESSAGE_TYPE_ASCII(1), MESSAGE_TYPE_UCS2(2)
  inMtNumber: Received number
  inMoNumber: My number
  outSMSPDU : PDU out buffer

  #define MAX_NUMBER_LENGTH 20
  #define MAX_MESSAGE_LENGTH 160

  EncodePDU("hello world!", 1, "01000000000", "01011111111", pduBuffer);
*/

int EncodePDU(/*IN*/char *inSMSTxt, /*IN*/int inType, /*IN*/char *inMtNumber, /*IN*/char *inMoNumber, /*OUT*/char *outSMSPDU)
{
  char strDstNumberBCD[MAX_NUMBER_LENGTH];
  char strOrgNumberBCD[MAX_NUMBER_LENGTH];

  char user_data_pdu[MAX_MESSAGE_LENGTH*2+4] = { '\0', };
  char user_data_raw[MAX_MESSAGE_LENGTH+2] = { '\0', };
  int  user_data_length = 0;

  char user_data_header_pdu[128] = { '\0', };
  int  i, msg_len = 0;

  int udhi = TP_MTI;
  int dcs = 0;
  int shift = 0;
  int udh_include = 0;
  WORD *user_data_ucs2 = NULL;


  // make BCD
  Hex2Bcd(inMtNumber,   strDstNumberBCD);
  Hex2Bcd(inMoNumber,   strOrgNumberBCD);

  if(MESSAGE_TYPE_UCS2 == inType)
  {
    user_data_ucs2 = (WORD *)inSMSTxt;
    
    for (i = 0; i < MAX_SMS_BYTES/2; i++)
    {
      if (user_data_ucs2[i] == 0) break;
    }

    msg_len = i;
    dcs = 0x8;
  }
  else
  {
    msg_len = (int)strlen(inSMSTxt);

	  for(i = 0; i < msg_len; i++)
	  {
	    if(0x80 & inSMSTxt[i])
	    {
	      // KS5601
	      dcs = 0x84;
	      strcpy(user_data_raw, inSMSTxt);
	      break;
	    }
	  }

    if (0 == dcs){
      msg_len = encode_alphabet(inSMSTxt, user_data_raw);
    }
  }


  if (udh_include)
  {
    char udh_1[60];
    char udh_2[60];
    sprintf(udh_1, "%02XA1%s", (int)strlen(inMoNumber), strOrgNumberBCD);
    sprintf(udh_2, "22%02X%s", (int)strlen(udh_1) / 2, udh_1);
    sprintf(user_data_header_pdu, "%02X%s", (int)strlen(udh_2) / 2, udh_2);
    udhi  = udhi | TP_UDHI;
  }

  if (0 == dcs)
  {
    int pos = 0;
    unsigned char out[MAX_MESSAGE_LENGTH+2] = {0, };

    i = 0;
    if (udhi & TP_UDHI)
    {
      int total_bits_occupied=0;
      
      // user_data_header => 0A22080B811020343136F1
      total_bits_occupied = (Hex2Dec(user_data_header_pdu) + 1) * 8;
      shift = (total_bits_occupied % 7);    /* fill_bits vary from 0 to 6 */

      if (shift != 0)
      {
        shift = 7 - shift;
      }

      user_data_length = (total_bits_occupied + shift + (msg_len * 7)) / 7;
    }else{
      user_data_length = msg_len;
    }

    /* pack the ASCII characters */
    shift %= 7;

    if (shift != 0)
    {
      /* Pack the upper bytes of the first character */
      out[pos] |= (unsigned char) (user_data_raw[i] << shift);
      shift = (7 - shift) + 1;
      if (shift == 7)
      {
        shift = 0;
        i++;
      }
      pos++;
    }

    for( ; pos < MAX_MESSAGE_LENGTH && i < msg_len;
         pos++, i++ )
    {
      /* pack the low bits */
      out[pos] = user_data_raw[i] >> shift;

      if( i+1 < msg_len )
      {
        /* pack the high bits using the low bits of the next character */
        out[pos] |= (unsigned char) (user_data_raw[i+1] << (7-shift));

        shift ++;

        if( shift == 7 )
        {
          shift = 0;
          i++;
        }
      }
    }

    for(i = 0; i < msg_len; i++)
    {
      sprintf(&user_data_pdu[i * 2], "%02X", out[i]);
    }
  }
  else if (0x8 == dcs)  // ucs2
  {
    for(i = 0; i < msg_len; i++)
    {
      sprintf(&user_data_pdu[i*4], "%04X", user_data_ucs2[i]);
    }
    user_data_length = msg_len*2;
  }
  else  // ksc5601
  {
    for(i = 0; i < msg_len; i++)
    {
      sprintf(&user_data_pdu[i*2], "%02X", (byte)user_data_raw[i]);
    }
    user_data_length = msg_len;
  }

  if (udhi & TP_UDHI)
  {
    msg_len = (int)sprintf(outSMSPDU, "00%02XFF%02XA1%s00%02X%02X%s%s", udhi, (int)strlen(inMtNumber), strDstNumberBCD, 
                                                          dcs, user_data_length, user_data_header_pdu, user_data_pdu);
  }
  else
  {
    msg_len = (int)sprintf(outSMSPDU, "00%02XFF%02XA1%s00%02X%02X%s", udhi, (int)strlen(inMtNumber), strDstNumberBCD, 
                                                          dcs, user_data_length, user_data_pdu);
  }

  return (msg_len/2 -1);
}


/*
 inSMSPDU: input SMS PDU
 outNumber : Sender number
 outSMSTxt: Decoded SMS message

*/
int DecodePDU(/*IN*/char *inSMSPDU, /*OUT*/int *outType, /*OUT*/char *outNumber, /*OUT*/char *outSMSTxt) 
{
  char user_data_raw[MAX_MESSAGE_LENGTH+4];

  char reply_number[MAX_NUMBER_LENGTH] = { 0, };
  char teleservice_id[10] = { 0, };

  int  i, len;
  int  pdu_index = 0;
  int  user_data_length = 0;
  int  user_data_header_length = 0;
  BOOL user_data_header_present;
  int  dcs;
  int  pid = 0;
  int  type_of_address;
  int  shift = 0;

  memset(user_data_raw, 0, sizeof(user_data_raw));

  // length of the SCA
  // 07
  len = Hex2Dec(&inSMSPDU[pdu_index]);
  pdu_index += 2;

  // SCA
  // 91280102194105
  pdu_index += len * 2;

  // check TP-UDHI bit (40)
  // 44
  if(TP_UDHI == (Hex2Dec(&inSMSPDU[pdu_index]) & TP_UDHI))
  {
    user_data_header_present = TRUE;
  }
  else
  {
    user_data_header_present = FALSE;
  }

  pdu_index += 2;

  // TP-OA
  // 0B
  len = Hex2Dec(&inSMSPDU[pdu_index]);
  pdu_index += 2;

  // A1
  type_of_address = Hex2Dec(&inSMSPDU[pdu_index]);
  pdu_index += 2;

  // 1020903746F0
  len = (len + (len % 2)) / 2;

  if (0 != len)
  {
    if (type_of_address & (TON_International << 4)){
      reply_number[0] = '+';
      Bcd2Hex(&inSMSPDU[pdu_index], reply_number+1, len);
    } else {
      Bcd2Hex(&inSMSPDU[pdu_index], reply_number, len);
    }
  }

  pdu_index = pdu_index + len * 2;

  // 00 : TP-PID
  pid = Hex2Dec(&inSMSPDU[pdu_index]);
  pdu_index += 2;
  
  // 00 : TP-DCS
  dcs = Hex2Dec(&inSMSPDU[pdu_index]);
  pdu_index += 2;

  DEBUG(MSG_HIGH, "DecodePDU pid %d, dcs %d\r\n", pid, dcs);

  if (pid != 0) {
    return -1;
  }
  
  // TP-SCTS
  // 21505251708263
  pdu_index += 14;

  // TP-UDL
  // 0E
  user_data_length = Hex2Dec(&inSMSPDU[pdu_index]);
  pdu_index += 2;

  if (user_data_header_present)
  {
    int  iei;
    int  udhl = 0;
    
    // TP-UDHL
    // 0A
    user_data_header_length = Hex2Dec(&inSMSPDU[pdu_index]);
    pdu_index += 2;

    // find 0x22 : Reply Address Element
    do
    {
      // IEI
      // 22
      iei = Hex2Dec(&inSMSPDU[pdu_index]);
      pdu_index += 2;

      // IEDL
      // 08
      len = Hex2Dec(&inSMSPDU[pdu_index]);
      pdu_index += 2;

      udhl += 2 + len;

      if(0x22 == iei)
      {
        // IED
        // 0B
        len = Hex2Dec(&inSMSPDU[pdu_index]);
        pdu_index += 2;

        // 81
        type_of_address = Hex2Dec(&inSMSPDU[pdu_index]);
        pdu_index += 2;

        // 1020903746F0
        len = (len + (len % 2)) / 2;

        if (0 != len)
        {
          memset(reply_number, 0, sizeof(reply_number));

          if (type_of_address & (TON_International << 4)){
            reply_number[0] = '+';
            Bcd2Hex(&inSMSPDU[pdu_index], reply_number+1, len);
          }else {
            Bcd2Hex(&inSMSPDU[pdu_index], reply_number, len);
          }
        }
      }
      else if(0x05 == iei)
      {
        // IED
        // 42020000
        memcpy(teleservice_id, &inSMSPDU[pdu_index], 4);
      }

      pdu_index += len * 2;
    }
    while(user_data_header_length != udhl);
  }

  if (0x00 == dcs)
  {
    int pos = 0;
    char user_data_pdu[MAX_MESSAGE_LENGTH*2+4];
    
    sprintf(user_data_pdu, "%s00", &inSMSPDU[pdu_index]);

    /*len would be the number of septets*/
    if(user_data_header_length > 0)
    {
      len = user_data_length;
      
      /*The number of fill bits required to make a septet boundary*/
      shift =( (len * 7) - ((user_data_header_length+1)*8) ) % 7;
      user_data_length = ( (len * 7) - ((user_data_header_length+1)*8) ) / 7;

      if (shift != 0)
      {
        shift = 8-shift;
      }
    }
    
    /*If the number of fill bits != 0, then it would cause an additional shift*/
    if (shift != 0)
     pos = pos+2;

    if (shift ==7)
    {
      user_data_raw[0] = Hex2Dec(&user_data_pdu[0]) >> 1; /*7 fillbits*/
      shift =0;            /*First Byte is a special case*/
      i=1;
    }else
      i = 0;

    for( i=i;
         i < MAX_MESSAGE_LENGTH && i< user_data_length;
         i++, pos += 2  )
    {
      user_data_raw[i] = ( Hex2Dec(&user_data_pdu[pos]) << shift ) & 0x7F;

      if( pos != 0 )
      {
        /* except the first byte, a character contains some bits
        ** from the previous byte.
        */
        user_data_raw[i] |= Hex2Dec(&user_data_pdu[pos-2]) >> (8-shift);
      }

      shift ++;

      if( shift == 7 )
      {
        shift = 0;

        /* a possible extra complete character is available */
        i ++;
        if( i >= MAX_MESSAGE_LENGTH )
        {
          user_data_length = MAX_MESSAGE_LENGTH;
          break;
        }
        user_data_raw[i] = Hex2Dec(&user_data_pdu[pos]) >> 1; 
      }
    }

    user_data_length = decode_alphabet(user_data_raw, user_data_length, user_data_raw);
  }
  else if (0x08 == dcs)
  {
    char *user_data_pdu = &inSMSPDU[pdu_index];

    len = (int)strlen(user_data_pdu);
    for(i = 0, user_data_length = 0; i < len; i += 4)
    {
      user_data_raw[i / 2] = Hex2Dec(&user_data_pdu[i+2]);
      user_data_raw[i / 2 + 1] = Hex2Dec(&user_data_pdu[i]);
      user_data_length +=2;
    }
  }
  else if (0x84 == dcs)
  {
    char *user_data_pdu = &inSMSPDU[pdu_index];

    len = (int)strlen(user_data_pdu);
    for(i = 0, user_data_length = 0; i < len; i += 2)
    {
      user_data_raw[i / 2] = Hex2Dec(&user_data_pdu[i]);
      user_data_length++;
    }
  }
  else
  {
    char *user_data_pdu = &inSMSPDU[pdu_index];

    len = (int)strlen(user_data_pdu);
    for(i = 0, user_data_length = 0; i < len; i += 2)
    {
      user_data_raw[i / 2] = Hex2Dec(&user_data_pdu[i]);
      user_data_length++;
      if (user_data_raw[i / 2] > 0x80) dcs = 0x84;
    }
  }

  for (i=0; i < strlen(reply_number); i++){
    if (reply_number[i] == 'A'){
      reply_number[i] = '*';
    }
    else if (reply_number[i] == 'B'){
      reply_number[i] = '#';
    }
    else if (reply_number[i] == 'C'){
      reply_number[i] = 'a';
    }
    else if (reply_number[i] == 'D'){
      reply_number[i] = 'b';
    }
    else if (reply_number[i] == 'E'){
      reply_number[i] = 'c';
    }
  }

  if (outNumber) {
    strcpy(outNumber, reply_number);
  }
  
  if (outSMSTxt) {
    if (dcs == 0x84 ){
      int charLen = GetCharLength((unsigned char*)user_data_raw, user_data_length);
      if ( KSC5601ToUCS2((unsigned char*)user_data_raw, (WORD*)outSMSTxt, charLen))
        user_data_length = charLen*2;
      else
        user_data_length = 0;
    }
    else
    {
      memcpy(outSMSTxt, user_data_raw, user_data_length);
    }
  }

  if (outType) {
    if (dcs == 0x84 || dcs == 0x8)
      *outType = MESSAGE_TYPE_UCS2;
    else
      *outType = MESSAGE_TYPE_ASCII;
  }
  
  return user_data_length;
}

