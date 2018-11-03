#include <string.h>
#include <stdlib.h>
#include <inttypes.h>
#include <avr/io.h>
#include <avr/pgmspace.h>
#include <avr/eeprom.h>


//-----------------------------------------
//              SECURITE
//             PIN /\ PUK
//-----------------------------------------

// prototype des fonctions d'entrée/sortie
// définies en assembleur dans le fichier io.s
void sendbytet0(uint8_t b);
uint8_t recbytet0(void);

// variables globales en static ram
uint8_t cla, ins, p1, p2, p3;  // header de commande
uint8_t sw1, sw2;              // status word

#define MAXI 16     // taille maxi des données lues
uint8_t data[MAXI]; // données introduites

#define HIST_STR	"hello sim"
#define HIST_SIZE	sizeof(HIST_STR)

//-----------------------------------------
//               DEFINE
//-----------------------------------------

#define PIN_SIZE 4
#define PUK_SIZE 8

#define VIERGE 0
#define LOCK 1
#define PIN_LOCKED 2

#define INIT_ATTEMPT 3

int is_unlock = 0;

//-----------------------------------------
//               EEPROM
//-----------------------------------------

uint8_t etat EEMEM = VIERGE;
uint8_t tentativesRestantes EEMEM = INIT_ATTEMPT;
uint8_t pin[PIN_SIZE] EEMEM = {0,0,0,0};
uint8_t puk[PUK_SIZE] EEMEM = {0,0,0,0,0,0,0,0};

//-----------------------------------------
//             MEMOIRE FLASH
//-----------------------------------------

// PROGMEM



//-----------------------------------------
//               FONCTIONS
//-----------------------------------------

// compare
int is_different(uint8_t* c1, uint8_t* c2, const uint8_t taille){
  int i;
  int res = 0;
  /* for(i = 0; i < taille; i++){
    if(c1[i] != c2[i]){
      return 1;
    }
  }*/
  for(i = 0; i < taille; i++)
  {
    res |= c1[i] ^ c2[i];
  }
  return res;
}

// Procédure qui renvoie l'ATR
void atr()
{
	int8_t n;
	char*hist;

	n=HIST_SIZE;
	hist=HIST_STR;
    	sendbytet0(0x3b); 	// définition du protocole
    	sendbytet0(n);		// nombre d'octets d'historique
    	while(n--)		// Boucle d'envoi des octets d'historique
    	{
        	sendbytet0(*hist++);
    	}
}

// commande d'introduction de code PUK
void intro_puk()
{
  int i;
  if(eeprom_read_byte(&etat) != VIERGE){
    sw1=0x6d; // commande inconnue
    return;
  }
  	// vérification de la taille
  if (p3 != PUK_SIZE)
  {
   	 sw1=0x6c;	// P3 incorrect
  	 sw2=PUK_SIZE;	// sw2 contient l'information de la taille correcte
	 return;
  }
  sendbytet0(ins);	// acquitement
  uint8_t tmp[PUK_SIZE];
	for(i = 0; i < p3; i++)	// boucle de réception du message
	{
	   tmp[i]=recbytet0();
	}
  for(i = 0; i < p3; i++){
    eeprom_write_byte(puk + i, tmp[i]);
  }
  for(i = 0; i < PIN_SIZE; i++){
    eeprom_write_byte(pin + i, 0x00);
  }
  eeprom_write_byte(&etat, LOCK);
  sw1 = 0x90;
  return;
}

// Card Holder Verification
void verify_CHV(){
  if( eeprom_read_byte(&etat) != LOCK || is_unlock == 1 ){
    sw1 = 0x6d; // commande inconnue
    return;
  }
  // vérification de la taille
  if( p3 != PIN_SIZE)
  {
    sw1 = 0x6c; // p3 incorrect
    sw2 = PIN_SIZE;
    return;
  }
  sendbytet0(ins); // acquittement
  int i = 0;
  uint8_t pin_tmp[PIN_SIZE];
  uint8_t pin_eeprom[PIN_SIZE];
  for(i = 0; i < PIN_SIZE; i++){
    pin_tmp[i] = recbytet0();
  }
  for(i = 0; i < PIN_SIZE; i++){
    pin_eeprom[i] = eeprom_read_byte(pin + i);
  }
  if(is_different(pin_tmp,pin_eeprom,PIN_SIZE)){
    uint8_t essai = eeprom_read_byte(&tentativesRestantes);
    essai--;
    eeprom_write_byte(&tentativesRestantes, essai);
    sw1 = 0x98;
    sw2 = 4*16+essai;
    if(essai == 0){
      eeprom_write_byte(&etat, PIN_LOCKED);
      sw2 = 0x50;
    }
    return;
  }
  eeprom_write_byte(&tentativesRestantes, INIT_ATTEMPT);
  is_unlock = 1;
  sw1 = 0x90;
}

void change_CHV(){
  if(!is_unlock || eeprom_read_byte(&etat) == PIN_LOCKED){
    sw1 = 0x6d; // commande inconnue
    return;
  }
  if( p3 != 2*PIN_SIZE ){
    sw1 = 0x6c; // taille incorrecte
    sw2 = 2*PIN_SIZE;
    return;
  }
  sendbytet0(ins); // acquittement
  int i;
  uint8_t chv_tmp[2*PIN_SIZE];
  uint8_t chv_eeprom[PIN_SIZE];
  for(i = 0; i < 2*PIN_SIZE; i++){
    chv_tmp[i] = recbytet0();
  }
  for(i = 0; i < PIN_SIZE; i++){
    chv_eeprom[i] = eeprom_read_byte(pin + i);
  }
  if(is_different(chv_tmp, chv_eeprom, PIN_SIZE)){
    sw1 = 0x98;
    sw2 = 0x4f;
    return;
  }else{
    for(i = 0 ; i < PIN_SIZE; i++){
      eeprom_write_byte(pin + i, chv_tmp[i+PIN_SIZE]);
    }
  }
  eeprom_write_byte(&etat, LOCK);
  sw1 = 0x90;
}

void unlock_CHV(){
  if(eeprom_read_byte(&etat) != PIN_LOCKED){
    sw1 = 0x6d; // commande inconnue
    return;
  }
  if( p3 != PUK_SIZE){
    sw1 = 0x6c; // taille incorrect
    sw2 = PUK_SIZE;
    return;
  }
  sendbytet0(ins); // acquittement
  int i;
  uint8_t puk_tmp[PUK_SIZE];
  uint8_t puk_eeprom[PUK_SIZE];
  for(i = 0; i < PUK_SIZE; i++){
    puk_tmp[i] = recbytet0();
  }
  for(i = 0; i < PUK_SIZE; i++){
    puk_eeprom[i] = eeprom_read_byte(puk + i);
  }
  if(is_different(puk_eeprom, puk_tmp, PUK_SIZE)){
    sw1 = 0x98;
    sw2 = 0x50;
    return;
  }
  eeprom_write_byte(&tentativesRestantes, INIT_ATTEMPT);
  // réinitialisation du PIN à 0000
  for(i = 0; i < PIN_SIZE; i++){
    eeprom_write_byte(pin + i, 0x00);
  }
  eeprom_write_byte(&etat, LOCK);
  sw1 = 0x90;
  return;
}

// programme principal
int main(void){
  //initialisation des ports
/*cours
  ACSR = 0x80;
  DDRA = 0xff;
  DDRB = 0xff;
  DDRC = 0xff;
  DDRD = 0x00;
  PORTA = 0xff;
  PORTB = 0xff;
  PORTC = 0xff;
  PORTD = 0xff;
*/
// arduino
  ACSR=0x80;
  DDRB=0xff;
  DDRC=0xff;
  DDRD=0;
  PORTB=0xff;
  PORTC=0xff;
  PORTD=0xff;
  ASSR=1<<EXCLK;
  TCCR2A=0;
  ASSR|=1<<AS2;


  is_unlock = 0;

  //ATR
  atr();

  sw2 = 0x00; // évite de le répéter dans toutes les cmds
  // boucle de traitement des commandes
  for(;;){
    sw2 = 0x00;
  //lecture de l'entête
    cla = recbytet0();
    ins = recbytet0();
    p1 = recbytet0();
    p2 = recbytet0();
    p3 = recbytet0();
    switch(cla){
      case 0xa0:
        switch(ins){
          case 0x00:
            intro_puk();
          break;
	     case 0x20:
            verify_CHV();
	     break;
	     case 0x24:
            change_CHV();
 	     break;
	     case 0x2c:
	          unlock_CHV();
 	     break;
          default:
            sw1 = 0x6d;
          break;
        }
      break;
      default:
        sw1 = 0x6e;
      break;
    }
    sendbytet0(sw1);
    sendbytet0(sw2);
  }
  return 0;
}
