#include <iostream>
#include <thread>
#include <deque>
#include <ncurses.h>
#include <mutex>
#include <unistd.h>
#include <condition_variable>
#include "People.h"
#include "Rooms.h"
#include <sys/ioctl.h>
#include <termios.h>
#define ESC 27

using namespace std;

int refreshingT = 100000;

const int hall_number = 5;  //liczba sal
const int counter_number = 3;   //liczba kas
const int controler_number = 5;
int viewers = 200; //liczba widzow w kinie

int free_cnts; //liczba wolnych kas
int free_ctrls; //liczba wolnych pracownikow, sprawdzajacych bilety
int visitors = 0;        //klienci, ktorzy juz wyszli
char key;       //przycisk do zamykania apliakcji

deque<Cinema_hall> halls; //sale kinowe

Toaleta WC;

deque <thread> t_clients;       //watki klientow
deque <thread> t_tickets;       //watki biletow

deque <Client> cnt_queue;       //kolejka do kas
deque <Client> ctrl_queue;      //kolejka do kontroli biletow

mutex m_tickets, m_counters;// m_toilet;      //mutexy
condition_variable cv_tickets, cv_counters;// cv_toilet;    //zmienne warunkowe

int row = 0;    //wiersz do ncurses
int timing =0;  //zmienna do symulacji czasu
int minutes = 0;
int hours = 0;

bool kbhit()
{
    termios term;
    tcgetattr(0, &term);

    termios term2 = term;
    term2.c_lflag &= ~ICANON;
    tcsetattr(0, TCSANOW, &term2);

    int byteswaiting;
    ioctl(0, FIONREAD, &byteswaiting);
    tcsetattr(0, TCSANOW, &term);

    return byteswaiting > 0;
}

//-------------------------------------------------------------------------------------------------------------
void init() {
    free_cnts = counter_number;         //domyslnie wszystkie kasy sa wolne
    free_ctrls = controler_number;      //domyslnie wszyscy kontrolerzy sa wolni

    for (int i = 0; i < viewers; i++) {
        cnt_queue.push_back(Client((rand() % 25) + 5, (rand() % 5))); //dodawanie klientow i losowanie czasu obslugi
    }
    for (int i = 0; i < hall_number; i++) {
        halls.push_back(Cinema_hall(i + 1));         //tworzenie sal kinowych 1-hall_number
    }
    initscr();
    curs_set(0);
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK); // zielony
        init_pair(2, COLOR_RED, COLOR_BLACK); // czerwony
        init_pair(3, COLOR_YELLOW, COLOR_BLACK); // zolty
    }

}
void buy_ticket() {     //zajmowanie kasy
    while (true) {
        lock_guard<mutex> lk(m_counters);
        if (!cnt_queue.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            cv_counters.notify_one();
        }
    }
}

void ticket_control() {     //sprawdzenie biletu
    while (true) {
        lock_guard<mutex> lk(m_tickets);
        if (!ctrl_queue.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            cv_tickets.notify_all();
        }
    }
}

void tickets() {
    unique_lock<mutex> lock1(m_tickets);    //lock_guard nie wspiera cv
    cv_tickets.wait(lock1, []() { return free_ctrls > 0; });  //cv +lambda expression

    free_ctrls--;

    Client guest = ctrl_queue.front();                //klient przechodzi z kolejki do kasy
    ctrl_queue.pop_front();                           //do kolejki do sprawdzenia biletu

    if (halls[guest.hall].population < halls[guest.hall].seats && !halls[guest.hall].movie) {
        halls[guest.hall].population++;                //klienci wchodza na sale jezeli nie jest zapelniona i
    } else {                                            //film sie jeszcze nie zaczal
        ctrl_queue.push_back(guest);              //klienci czekaja w kolejce przed salami
    }
    lock1.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(10)); //zblokowanie biletow

    lock1.lock();

    free_ctrls++;       //zwolnienie kontrolera
    lock1.unlock();
}

void client() {
    unique_lock<mutex> lock1(m_counters);
    cv_counters.wait(lock1, []() { return free_cnts > 0; });    //cv + lambda expression

    Client guest = Client();
    free_cnts--;    //zajecie kasy

    if (!cnt_queue.empty()) {
        guest = cnt_queue.front();
        cnt_queue.pop_front();
    }
    lock1.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(guest.trans_time*5));

    lock1.lock();   //zblokowanie kasy
    free_cnts++;    //zwolnienie kasy
    ctrl_queue.push_back(guest);    //przerzucenie klienta do kolejki na sale
    lock1.unlock();     //blokujemy kase
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    m_tickets.lock();
    t_tickets.push_back(thread((tickets)));
    t_tickets.back().detach();
    m_tickets.unlock();
}
//-------------------------------------------------------------------------------------------------------------
void ncurses_rysuj() {
    //-------------------------------------Rysowanie Sal-----------------------------------------------------------
    attron(COLOR_PAIR(1));
    for(int i=0; i<hall_number; i++){
        row = 0;
        mvprintw(row,i*16, "Sala nr " );
        printw(to_string(halls[i].nr).c_str()); row++; attron(COLOR_PAIR(1));
        mvprintw(row,i*16, "-------------" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "-------------" );row++;

        if(halls[i].seats==halls[i].population) {
            mvprintw(row,i*16, "Pelna sala"); row++;}
        else {
            mvprintw(row,i*16, "Osob:");
            printw(to_string(halls[i].population).c_str());row++;
        }
        if(halls[i].movie){ mvprintw(row,i*16, "Leci film"); }
        else{ mvprintw(row,i*16, "Nie ma filmu"); }
    }
    attroff(COLOR_PAIR(1));
    //----------------------------kolejka do kontroli-------------------------------------------------------------
    row =12;
    for (int i = 1; i < free_ctrls + 1; i++) {
        attron(COLOR_PAIR(1));
        mvprintw(row, 0, "(wolny) X "); //kolor zielony dla wolnej kasy
        row++;
        attroff(COLOR_PAIR(1));
    }
    for (int i = free_ctrls + 1; i <= 5 ; i++) {
        attron(COLOR_PAIR(2));
        mvprintw(row, 0, "(zajety) X @"); //czerwony dla zajetych kas
        row++;
        attroff(COLOR_PAIR(2));
    }
    unsigned dlugosc_na_sale = ctrl_queue.size();
    mvprintw(row, 0, "Kolejka do kontroli: ");
    printw(to_string(dlugosc_na_sale).c_str()); row++;
    row++;
    if (dlugosc_na_sale > 100) {        //ograniczenie dlugosci na rzecz rysowania
        dlugosc_na_sale = 100;
    }
    for (int i = 0; i < dlugosc_na_sale; i++) {
        mvprintw(row - 1, 50 + i, "@");
    }
    row++;
    //-------------------------------------Rysowanie WC-----------------------------------------------------------
        attron(COLOR_PAIR(3));
            mvprintw(row,0, "Toaleta " ); row++;
            mvprintw(row,0, "-------------" );row++;
            mvprintw(row,0, "|            |" );row++;
            mvprintw(row,0, "|    W       |" );row++;
            mvprintw(row,0, "|    C       |" );row++;
            mvprintw(row,0, "|            |" );row++;
            mvprintw(row,0, "|            |" );row++;
            mvprintw(row,0, "-------------" );row++;
        attroff(COLOR_PAIR(3));
        row++;
    //----------------------------rysowanie kas---------------------------------------------------------------

    for (int i = 1; i < free_cnts+1; i++) {
            attron(COLOR_PAIR(1));
            mvprintw(row, 0, "(wolna) KASA "); //kolor zielony dla wolnej kasy
            row++;
            attroff(COLOR_PAIR(1));
        }
        for (int i = free_cnts+1; i <= counter_number; i++) {
            attron(COLOR_PAIR(2));
            mvprintw(row, 0, "(zajeta) KASA @"); //czerwony dla zajetych kas
            row++;
            attroff(COLOR_PAIR(2));
    }
    row++;

    //----------------------------kolejka do kasy-------------------------------------------------------------
    int dlugosc_kolejki = cnt_queue.size();
    mvprintw(row, 0, "Kolejka do kasy: ");
    printw(to_string(dlugosc_kolejki).c_str()); row++;

    if (dlugosc_kolejki > 100) {        //ograniczenie dlugosci na rzecz rysowania
        dlugosc_kolejki = 100;
    }
    for (int i = 0; i < dlugosc_kolejki; i++) {
        mvprintw(row - 1, 50 + i, "@");
    }
    row++;

    //----------------------------rysowanie zegara---------------------------------------------------------------
    string hh;
    string mm;

    if (hours == 0) {
        hh = "0";
    } else {
        hh = to_string(hours);
    }
    if (minutes == 0) {
        mm = "00";
    } else if (minutes < 10) {
        mm = "0" + to_string(minutes);
    } else {
        mm = to_string(minutes);
    }

    string clk = "Czas symulacji: " + hh + ":" + mm;

    mvprintw(row, row/2, clk.c_str()); row++;
    mvprintw(row, (row-1)/2, "liczba ludzi, ktorzy zakonczyli seans: ");
    printw(to_string(visitors).c_str());
}
//-------------------------------------------------------------------------------------------------------------

void time_simulation() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(600));
        timing++;

        if (timing == 15) {
            halls[0].movie = true;   //po 15 minutach zaczyna sie film w sali 0
        }
        if (timing == 30) {
            halls[1].movie = true;   //po 30 minutach zaczyna sie film w sali 1
        }
        if (timing == 60) {
            halls[2].movie = true;   //po 60 minutach zaczyna sie film w sali 2
        }
        if (timing == 90) {
            halls[3].movie = true;   //po 90 minutach zaczyna sie film w sali 3
        }
        if (timing == 120) {
            halls[0].movie = false;  //konczy sie film w sali 0,
            halls[0].end_show = true;

            halls[4].movie = true;   //zaczyna w sali 4
        }
        if (timing == 135) {
            halls[0].end_show = false;    //ludzie wyszli z sali 0

            halls[1].movie = false;      //w sali 1 skonczyl sie film
            halls[1].end_show = true;    //ludzie wychodza z sali 1
        }
        if (timing == 150) {
            halls[0].movie = true;   //w sali 0 zaczyna sie kolejny film

            halls[1].end_show = false;    //ludzie wyszli z sali 1
        }
        if (timing == 180) {
            halls[1].movie = true; //w sali 1 zaczyna sie nowy film

            halls[2].movie = false; //w sali 2 skonczyl sie film
            halls[2].end_show = true;

        }
        if (timing == 195) {
            halls[2].end_show = false; //ludzie wyszli z sali 2

            halls[3].movie = false;  //w sali 3 skonczyl sie film
            halls[3].end_show = true; //ludzie wychodza z sali 3
        }
        if (timing == 210) {
            halls[3].end_show = false; //ludzie wyszli z sali 3

            halls[4].movie = false; //w sali 4 konczy sie film
            halls[4].end_show = true; //ludzie wyszli z sali 4

        }
        if (timing == 225) {
            halls[4].end_show = false;

            halls[0].movie = false;
            halls[0].end_show = true;
        }
        if (timing == 240) {
            halls[0].end_show = false;
            halls[1].movie = false;
            halls[1].end_show = true;
        }
        if (timing == 254) {
            halls[1].end_show = false; //przed zapetleniem sie czasu wszystkie filmy sie koncza a sale pozostaja puste
        }
        minutes = timing % 60;
        timing = timing % 255;
        if (minutes == 0) {
            hours++;
        }
        m_tickets.lock();
        for (int i = 0; i < hall_number; i++) {
            if (halls[i].end_show) {
                while (halls[i].population > 0) {
                    halls[i].population--;
                    visitors++;
                }
            }
        }
        m_tickets.unlock();
    }
}

int main() {
    srand(time(NULL));
    init();                        //startujemy

    thread t_free_counter = thread(buy_ticket);     //startowanie watkow
    thread t_free_controler = thread(ticket_control);
    for (int i = 0; i < viewers; i++) {           //budowanie kolejki klientow
        t_clients.push_back(thread(client));
    }
    thread t_timing = thread(time_simulation);

    while (true) {
        if (kbhit()) {
            key = getch();
            if (key == ESC) {
                t_free_counter.join();
                t_free_controler.join();
                for (int i = 0; i < viewers; i++) {
                    t_clients[i].join();
                }
                t_timing.detach();

                getch();
                endwin();
                return 0;
            }
        }

        clear();
        ncurses_rysuj();

        refresh();
        usleep(refreshingT);
    }
}