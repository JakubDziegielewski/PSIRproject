#include <Arduino.h>
#include <ZsutDhcp.h>        //dla pobierania IP z DHCP - proforma dla ebsim'a
#include <ZsutEthernet.h>    //niezbedne dla klasy 'ZsutEthernetUDP'
#include <ZsutEthernetUdp.h> //sama klasa 'ZsutEthernetUDP'
#include <ZsutFeatures.h>    //for: ZsutMillis()

#define MAX_NUMBER_OF_NODES 3
#define UDP_SERVER_PORT 1234
#define UDP_CLIENT_PORT 1234
#define STATE_MESSAGE 0b01000000U
#define HELLO_MESSAGE 0b10000000U
#define ACK_MESSAGE 0b11000000U
#define AWAIT_STATE 0
#define SENDING_NEIGHBORS 1
#define WAIT_FOR_CELLS 2
#define SIZE 6

uint8_t number_of_nodes;
uint8_t current_state;
uint16_t time;
ZsutEthernetUDP Udp;
char packet_buffer[100];
byte mac[] = {0x08, 0x00, 0x27, 0xc1, 0x6e, 0xf6};
struct cell
{
  uint8_t x_coordinate;
  uint8_t y_coordinate;
  uint8_t state;
};
struct node
{
  cell first_cell;
  cell last_cell;
  ZsutIPAddress node_addr;
};
node remote_node;
node list_of_nodes[MAX_NUMBER_OF_NODES];
cell temp_cell;
cell list_of_cells[SIZE][SIZE];
uint8_t seed[36] = {1, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 0, 0, 0, 1, 0, 1, 1, 1, 1, 0, 0};
void setup()
{
  Serial.begin(115200);
  ZsutEthernet.begin(mac);
  Udp.begin(UDP_SERVER_PORT);
  current_state = AWAIT_STATE;
  for (size_t i = 0; i < SIZE; i++)
  {
    for (size_t j = 0; j < SIZE; j++)
    {
      list_of_cells[i][j].x_coordinate = i;
      list_of_cells[i][j].y_coordinate = j;
      list_of_cells[i][j].state = seed[(SIZE * i + j) % 36];
    }
  }
  time = ZsutMillis();
  number_of_nodes = 0;
}

void loop()
{
  if (current_state == AWAIT_STATE)
  {

    if (ZsutMillis() - time > 4000)
    {
      time = ZsutMillis();
      Serial.println("Waiting for 6 clients");
    }

    int packet_size = Udp.parsePacket();
    if (packet_size)
    {
      Udp.read(packet_buffer, packet_size);
      if (packet_buffer[0] & HELLO_MESSAGE)
      {
        remote_node.node_addr = Udp.remoteIP();
        Serial.println("Received hello message");
        Serial.println(remote_node.node_addr);
        switch (number_of_nodes)
        {
        case 0:
          temp_cell.x_coordinate = 0;
          temp_cell.y_coordinate = 0;
          remote_node.first_cell = temp_cell;
          temp_cell.x_coordinate = 5;
          temp_cell.y_coordinate = 5;
          remote_node.last_cell = temp_cell;
          break;
        case 1:
          temp_cell.x_coordinate = 0;
          temp_cell.y_coordinate = 3;
          remote_node.first_cell = temp_cell;
          temp_cell.x_coordinate = 2;
          temp_cell.y_coordinate = 5;
          remote_node.last_cell = temp_cell;
          break;
        case 2:
          temp_cell.x_coordinate = 3;
          temp_cell.y_coordinate = 0;
          remote_node.first_cell = temp_cell;
          temp_cell.x_coordinate = 5;
          temp_cell.y_coordinate = 5;
          remote_node.last_cell = temp_cell;
          break;
        case 3:
          temp_cell.x_coordinate = 3;
          temp_cell.y_coordinate = 0;
          remote_node.first_cell = temp_cell;
          temp_cell.x_coordinate = 5;
          temp_cell.y_coordinate = 5;
          remote_node.last_cell = temp_cell;
          break;
        case 4:
          temp_cell.x_coordinate = 4;
          temp_cell.y_coordinate = 0;
          remote_node.first_cell = temp_cell;
          temp_cell.x_coordinate = 5;
          temp_cell.y_coordinate = 1;
          remote_node.last_cell = temp_cell;
          break;
        case 5:
          temp_cell.x_coordinate = 4;
          temp_cell.y_coordinate = 2;
          remote_node.first_cell = temp_cell;
          temp_cell.x_coordinate = 5;
          temp_cell.y_coordinate = 5;
          remote_node.last_cell = temp_cell;
          break;
        }
        list_of_nodes[number_of_nodes] = remote_node;
        number_of_nodes = number_of_nodes + 1;
        if (number_of_nodes == MAX_NUMBER_OF_NODES)
        {
          for (size_t i = 0; i < MAX_NUMBER_OF_NODES; i++)
          {
            packet_buffer[0] = ACK_MESSAGE;
            packet_buffer[1] = (uint8_t)6;
            packet_buffer[2] = list_of_nodes[i].first_cell.x_coordinate;
            packet_buffer[3] = list_of_nodes[i].first_cell.y_coordinate;
            packet_buffer[4] = list_of_nodes[i].last_cell.x_coordinate;
            packet_buffer[5] = list_of_nodes[i].last_cell.y_coordinate;

            Udp.beginPacket(list_of_nodes[i].node_addr, UDP_CLIENT_PORT);
            Udp.write(packet_buffer, 6);
            Udp.endPacket();
            memset(&packet_buffer, 0, sizeof(packet_buffer));
            Serial.print("Sent ack message to:");
            Serial.print(list_of_nodes[i].node_addr);
            Serial.print("\n");
          }
          for (uint8_t i = 0; i < MAX_NUMBER_OF_NODES; i++)
          {
            packet_buffer[0] = STATE_MESSAGE;
            remote_node = list_of_nodes[i];
            Serial.println("Sending seed");
            uint8_t index_in_buffer = 2;
            for (uint8_t j = remote_node.first_cell.x_coordinate; j <= remote_node.last_cell.x_coordinate; j++)
            {
              for (uint8_t k = remote_node.first_cell.y_coordinate; k <= remote_node.last_cell.y_coordinate; k++)
              {
                byte temp = (j << 5);
                temp = (temp | k << 2);
                temp = (temp | list_of_cells[j][k].state);
                packet_buffer[index_in_buffer] = temp;
                index_in_buffer++;
              }
            }
            packet_buffer[1] = index_in_buffer;
            Udp.beginPacket(remote_node.node_addr, UDP_CLIENT_PORT);
            Udp.write(packet_buffer, index_in_buffer);
            Udp.endPacket();
            memset(&packet_buffer, 0, sizeof(packet_buffer));
          }
          current_state = SENDING_NEIGHBORS;
        }
      }
      else
      {
        // error handling
      }
    }
  }
  else if (current_state == SENDING_NEIGHBORS)
  {

    if (ZsutMillis() - time >= 4000) // nowa runda
    {
      memset(&packet_buffer, 0, sizeof(packet_buffer));
      time = ZsutMillis();
      for (uint8_t i = 0; i < SIZE; i++)
      {
        for (uint8_t j = 0; j < SIZE; j++)
        {
          if (list_of_cells[j][i].state == 1)
          {
            Serial.print("O");
          }
          else
          {
            Serial.print("X");
          }
        }
        Serial.println("");
      }
      delay(2000);
      Serial.println("Sending neighbors");

      for (uint8_t i = 0; i < MAX_NUMBER_OF_NODES; i++)
      {
        remote_node = list_of_nodes[i];
        packet_buffer[0] = STATE_MESSAGE;
        uint8_t index_in_buffer = 2;
        if (remote_node.first_cell.x_coordinate != 0)
        {
          uint8_t column = remote_node.first_cell.x_coordinate - 1;
          for (uint8_t j = remote_node.first_cell.y_coordinate; j <= remote_node.last_cell.y_coordinate; j++)
          {
            byte temp = (column << 5);
            temp = (temp | (j << 2));
            temp = (temp | list_of_cells[column][j].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
          if (remote_node.first_cell.y_coordinate != 0)
          {
            byte temp = (column << 5);
            temp = (temp | ((remote_node.first_cell.y_coordinate - (uint8_t)1) << 2));
            temp = (temp | list_of_cells[column][remote_node.first_cell.y_coordinate - 1].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
          if (remote_node.last_cell.y_coordinate != SIZE - 1)
          {
            byte temp = (column << 5);
            temp = (temp | ((remote_node.last_cell.y_coordinate + (uint8_t)1) << 2));
            temp = (temp | list_of_cells[column][remote_node.last_cell.y_coordinate + 1].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
        }
        if (remote_node.first_cell.y_coordinate != 0)
        {
          uint8_t row = remote_node.first_cell.y_coordinate - 1;
          for (uint8_t j = remote_node.first_cell.x_coordinate; j <= remote_node.last_cell.x_coordinate; j++)
          {
            byte temp = (j << 5);
            temp = (temp | (row << 2));
            temp = (temp | list_of_cells[j][row].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
          if (remote_node.first_cell.x_coordinate != 0)
          {
            byte temp = ((remote_node.first_cell.x_coordinate - (uint8_t)1) << 5);
            temp = (temp | (row << 2));
            temp = (temp | list_of_cells[remote_node.first_cell.x_coordinate - 1][row].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
          if (remote_node.last_cell.x_coordinate != SIZE - 1)
          {
            byte temp = ((remote_node.last_cell.x_coordinate + (uint8_t)1) << 5);
            temp = (temp | (row << 2));
            temp = (temp | list_of_cells[remote_node.last_cell.x_coordinate + 1][row].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
        }
        if (remote_node.last_cell.x_coordinate != SIZE - 1)
        {
          uint8_t column = remote_node.last_cell.x_coordinate + 1;
          for (uint8_t j = remote_node.first_cell.y_coordinate; j <= remote_node.last_cell.y_coordinate; j++)
          {
            byte temp = (column << 5);
            temp = (temp | (j << 2));
            temp = (temp | list_of_cells[column][j].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
          if (remote_node.first_cell.y_coordinate != 0)
          {
            byte temp = (column << 5);
            temp = (temp | ((remote_node.first_cell.y_coordinate -(uint8_t)1) << 2));
            temp = (temp | list_of_cells[column][remote_node.first_cell.y_coordinate - 1].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
          if (remote_node.last_cell.y_coordinate != SIZE - 1)
          {
            byte temp = (column << 5);
            temp = (temp | (remote_node.last_cell.y_coordinate + (uint8_t)1) << 2);
            temp = (temp | list_of_cells[column][remote_node.last_cell.y_coordinate + 1].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
        }
        if (remote_node.last_cell.y_coordinate != SIZE - 1)
        {
          uint8_t row = remote_node.last_cell.y_coordinate + 1;
          for (uint8_t j = remote_node.first_cell.x_coordinate; j <= remote_node.last_cell.x_coordinate; j++)
          {
            byte temp = (j << 5);
            temp = (temp | (row << 2));
            temp = (temp | list_of_cells[j][row].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
           if (remote_node.first_cell.x_coordinate != 0)
          {
            byte temp = ((remote_node.first_cell.x_coordinate - (uint8_t)1) << 5);
            temp = (temp | (row << 2));
            temp = (temp | list_of_cells[remote_node.first_cell.x_coordinate - 1][row].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
          if (remote_node.last_cell.x_coordinate != SIZE - 1)
          {
            byte temp = ((remote_node.last_cell.x_coordinate + (uint8_t)1) << 5);
            temp = (temp | (row << 2));
            temp = (temp | list_of_cells[remote_node.last_cell.x_coordinate + 1][row].state);
            packet_buffer[index_in_buffer] = temp;
            index_in_buffer++;
          }
        }
        packet_buffer[1] = index_in_buffer;
        Udp.beginPacket(remote_node.node_addr, UDP_CLIENT_PORT);
        Udp.write(packet_buffer, index_in_buffer);
        Udp.endPacket();
      }
      current_state = WAIT_FOR_CELLS;
    }
  }
  else if (current_state == WAIT_FOR_CELLS)
  {
    Serial.println("Waiting for cells");
    uint8_t recieved_messages = 0;
    while (recieved_messages < MAX_NUMBER_OF_NODES || ZsutMillis() - time > 6000)
    {
      int packet_size = Udp.parsePacket();
      if (packet_size)
      {
        Udp.read(packet_buffer, packet_size);
        if (packet_buffer[0] & STATE_MESSAGE)
        {
          uint8_t length = (uint8_t)packet_buffer[1];
          for (size_t i = 2; i < length; i++)
          {
            uint8_t x = (uint8_t)((packet_buffer[i] & 0b11100000U) >> 5);
            uint8_t y = (uint8_t)((packet_buffer[i] & 0b00011100U) >> 2);
            uint8_t s = (uint8_t)(packet_buffer[i] & 0b00000001U);
            list_of_cells[x][y].state = s;
          }
        }
        recieved_messages++;
        Serial.println("Recieved cells from a node");
      }
    }
    current_state = SENDING_NEIGHBORS;
  }
}
