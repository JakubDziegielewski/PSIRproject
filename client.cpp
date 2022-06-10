#include <Arduino.h>
#include <ZsutDhcp.h>        //dla pobierania IP z DHCP - proforma dla ebsim'a
#include <ZsutEthernet.h>    //niezbedne dla klasy 'ZsutEthernetUDP'
#include <ZsutEthernetUdp.h> //sama klasa 'ZsutEthernetUDP'
#include <ZsutFeatures.h>    //for: ZsutMillis()

#define UDP_SERVER_PORT 1234
#define UDP_CLIENT_PORT 1234
#define STATE_MESSAGE 0b01000000U
#define HELLO_MESSAGE 0b10000000U
#define ACK_MESSAGE 0b11000000U
#define SERVER_IP 192, 168, 56, 111
#define AWAIT_STATE 0
#define AWAIT_FOR_NEIGHBORS 1
#define CALCULATING_NEXT_STEP 2
#define SENDING_CELLS 3
#define SIZE 6
unsigned char packet_buffer[40];
byte MAC[] = {0x08, 0x00, 0x27, 0x08, 0x2a, 0x7a}; // MAC adres karty sieciowej, to powinno byc unikatowe - proforma dla ebsim'a
struct cell
{
  uint8_t x_coordinate;
  uint8_t y_coordinate;
  uint8_t state;
  uint8_t alive_neighbors;
};
struct node
{
  cell first_cell;
  cell last_cell;
  ZsutIPAddress node_addr;
};
struct node self;
ZsutIPAddress server_ip;
ZsutEthernetUDP Udp;
uint16_t time;
uint8_t current_state;
uint8_t number_of_cells;
cell list_of_cells[SIZE][SIZE];

void setup()
{
  Serial.begin(115200);
  ZsutEthernet.begin(MAC);
  server_ip = ZsutIPAddress(SERVER_IP);
  Udp.begin(UDP_SERVER_PORT);
  time = ZsutMillis();
  packet_buffer[0] = HELLO_MESSAGE;

  Udp.beginPacket(server_ip, UDP_SERVER_PORT);
  Udp.write(packet_buffer, 2);
  Udp.endPacket();
  current_state = AWAIT_STATE;
};

void loop()
{
  if (current_state == AWAIT_STATE)
  {
    uint8_t recieved_ack = 0;
    while (!recieved_ack)
    {
      int packet_size = Udp.parsePacket();
      if (packet_size)
      {
        Udp.read(packet_buffer, packet_size);
        if (packet_buffer[0] & ACK_MESSAGE)
        {
          Serial.println("Recieved ACK message");
          self.first_cell.x_coordinate = (uint8_t)packet_buffer[2];
          self.first_cell.y_coordinate = (uint8_t)packet_buffer[3];
          self.last_cell.x_coordinate = (uint8_t)packet_buffer[4];
          self.last_cell.y_coordinate = (uint8_t)packet_buffer[5];
          number_of_cells = (self.last_cell.x_coordinate - self.first_cell.x_coordinate + 1) * (self.last_cell.y_coordinate - self.first_cell.y_coordinate + 1);
          Serial.print(self.first_cell.x_coordinate);
          Serial.print(self.first_cell.y_coordinate);
          Serial.print(self.last_cell.x_coordinate);
          Serial.println(self.last_cell.y_coordinate);
          recieved_ack = 1;
        }
      }
    }

    uint8_t recieved_cells = 0;
    while (!recieved_cells)
    {
      int packet_size = Udp.parsePacket();
      if (packet_size)
      {
        Udp.read(packet_buffer, packet_size);
        if (packet_buffer[0] & STATE_MESSAGE)
        {
          uint8_t length = (uint8_t)packet_buffer[1];
          Serial.print("Length of a message: ");
          Serial.println(length);
          for (uint8_t i = 2; i < length; i++)
          {
            uint8_t x = ((0b11100000 & packet_buffer[i]) >> 5);
            uint8_t y = ((0b00011100 & packet_buffer[i]) >> 2);
            uint8_t s = (0b00000011 & packet_buffer[i]);
            list_of_cells[x][y].state = s;
          }
          recieved_cells = 1;
        }
      }
    }
    current_state = AWAIT_FOR_NEIGHBORS;
  }
  else if (current_state == AWAIT_FOR_NEIGHBORS)
  {
    int packet_size = Udp.parsePacket();
    if (packet_size)
    {
      Udp.read(packet_buffer, packet_size);
      if ((uint8_t)packet_buffer[0] == (uint8_t)STATE_MESSAGE)
      {
        Serial.print("Recieved a msg, packet size: ");
        Serial.println(packet_size);
        uint8_t length = (uint8_t)packet_buffer[1];
        for (uint8_t i = 2; i < length; i++)
        {
          uint8_t x = (uint8_t)((0b11100000 & packet_buffer[i]) >> 5);
          uint8_t y = (uint8_t)((0b00011100 & packet_buffer[i]) >> 2);
          uint8_t s = (uint8_t)(0b00000001 & packet_buffer[i]);
          list_of_cells[x][y].state = s;
        }
      }
      Serial.println("\nFinished recieving nieghbors");
      current_state = CALCULATING_NEXT_STEP;
    }
  }

  else if (current_state == CALCULATING_NEXT_STEP)
  {
    for (uint8_t i = self.first_cell.x_coordinate; i <= self.last_cell.x_coordinate; i++)
    {
      for (uint8_t j = self.first_cell.y_coordinate; j <= self.last_cell.y_coordinate; j++)
      {
        list_of_cells[i][j].alive_neighbors = 0;
        if (i > 0 && i < SIZE - 1 && j > 0 && j < SIZE - 1)
        {
          for (uint8_t k = i - 1; k <= i + 1; k++)
          {
            for (uint8_t l = j - 1; l <= j + 1; l++)
            {
              list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
            }
          }
        }
        else if (i == 0)
        {
          if (j == 0)
          {
            list_of_cells[i][j].alive_neighbors += 2 * ZsutAnalog0Read() + 2 * ZsutAnalog3Read();
            for (uint8_t k = i; k < i + 2; k++)
            {
              for (uint8_t l = j; l < j + 2; l++)
              {
                list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
              }
            }
          }
          else if (j == SIZE - 1)
          {
            list_of_cells[i][j].alive_neighbors += 2 * ZsutAnalog2Read() + 2 * ZsutAnalog3Read();
            for (uint8_t k = i; k < i + 2; k++)
            {
              for (uint8_t l = j - 1; l < j + 1; l++)
              {
                list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
              }
            }
          }
          else
          {
            list_of_cells[i][j].alive_neighbors += 3 * ZsutAnalog3Read();
            for (uint8_t k = i; k < i + 2; k++)
            {
              for (uint8_t l = j - 1; l < j + 2; l++)
              {
                list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
              }
            }
          }
        }
        else if (i == SIZE - 1)
        {
          if (j == 0)
          {
            list_of_cells[i][j].alive_neighbors += 2 * ZsutAnalog0Read() + 2 * ZsutAnalog1Read();
            for (uint8_t k = i - 1; k < i + 1; k++)
            {
              for (uint8_t l = j; l < j + 2; l++)
              {
                list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
              }
            }
          }
          else if (j == SIZE - 1)
          {
            list_of_cells[i][j].alive_neighbors += 2 * ZsutAnalog2Read() + 2 * ZsutAnalog1Read();
            for (uint8_t k = i - 1; k < i + 1; k++)
            {
              for (uint8_t l = j - 1; l < j + 1; l++)
              {
                list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
              }
            }
          }
          else
          {
            list_of_cells[i][j].alive_neighbors += 3 * ZsutAnalog1Read();
            for (uint8_t k = i - 1; k < i + 1; k++)
            {
              for (uint8_t l = j - 1; l < j + 2; l++)
              {
                list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
              }
            }
          }
        }
        else if (j == 0)
        {
          list_of_cells[i][j].alive_neighbors += 3 * ZsutAnalog0Read();
          for (uint8_t k = i - 1; k < i + 2; k++)
          {
            for (uint8_t l = j; l < j + 2; l++)
            {
              list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
            }
          }
        }
        else if (j == SIZE - 1)
        {
          list_of_cells[i][j].alive_neighbors += 3 * ZsutAnalog2Read();
          for (uint8_t k = i - 1; k < i + 2; k++)
          {
            for (uint8_t l = j - 1; l < j + 1; l++)
            {
              list_of_cells[i][j].alive_neighbors += list_of_cells[k][l].state;
            }
          }
        }

        if (list_of_cells[i][j].state)
        {
          list_of_cells[i][j].alive_neighbors--;
        }
      }
    }

     for (uint8_t i = self.first_cell.x_coordinate; i <= self.last_cell.x_coordinate; i++)
     {
       for (uint8_t j = self.first_cell.y_coordinate; j <= self.last_cell.y_coordinate; j++)
       {

         if ((list_of_cells[i][j].alive_neighbors >= 3 || list_of_cells[i][j].alive_neighbors < 1) && list_of_cells[i][j].state == 1)
         {
           list_of_cells[i][j].state = 0;
         }
         else if ((list_of_cells[i][j].alive_neighbors > 1 && list_of_cells[i][j].alive_neighbors < 5) && list_of_cells[i][j].state == 0)
         {
           list_of_cells[i][j].state = 1;
         }
       }
     }
    

    Serial.println("\nCells calculated");
    current_state = SENDING_CELLS;
  }
  else if (current_state == SENDING_CELLS)
  {
    Serial.println("sending calculated cells");
    packet_buffer[0] = STATE_MESSAGE;

    uint8_t index_in_buffer = 2;
    for (uint8_t j = self.first_cell.x_coordinate; j <= self.last_cell.x_coordinate; j++)
    {
      for (uint8_t k = self.first_cell.y_coordinate; k <= self.last_cell.y_coordinate; k++)
      {
        byte temp = 0b00000000U;
        temp = (j << 5);
        temp = (temp | k << 2);
        temp = (temp | list_of_cells[j][k].state);
        packet_buffer[index_in_buffer] = temp;
        index_in_buffer++;
      }
    }
    packet_buffer[1] = index_in_buffer;
    Udp.beginPacket(server_ip, UDP_SERVER_PORT);
    Udp.write(packet_buffer, index_in_buffer);
    Udp.endPacket();
    memset(&packet_buffer, 0, sizeof(packet_buffer));
    current_state = AWAIT_FOR_NEIGHBORS;
  }
}