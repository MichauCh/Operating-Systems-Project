#include <iostream>
#include <thread>
#include <deque>
#include <ncurses.h>
#include <mutex>
#include <unistd.h>
#include <condition_variable>
#include "Ludzie.h"
#include "Pomieszczenia.h"
#include <sys/ioctl.h>
#include <termios.h>
#define ESC 27

using namespace std;

int refreshingT = 100000;

const int hall_number = 5;  //liczba sal
const int counter_number = 3;   //liczba kas
int viewers = 200; //liczba widzow w kinie

int free_cnts =1; //liczba wolnych kas
int free_ctrls; //liczba wolnych pracownikow, sprawdzajacych bilety
int visitors = 0;        //klienci, ktorzy juz wyszli
char key;       //przycisk do zamykania apliakcji

deque<Cinema_hall> sale; //sale kinowe

Toaleta WC;

deque <thread> t_clients;       //watki klientow
deque <thread> t_tickets;       //watki biletow

deque <Client> cnt_queue;       //kolejka do kas
deque <Client> ctrl_queue;      //kolejka do kontroli biletow

mutex m_tickets, m_counters, m_toilet;

condition_variable cv_tickets, cv_counters, cv_toilet;

int col = 0;
int row = 0;


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
    free_cnts = counter_number;
    free_ctrls = 5;

    for (int i = 0; i < viewers; i++) {
        cnt_queue.push_back(Client((rand() % 30) + 1, (rand() % 5))); //dodawanie klientow
    }

    for (int i = 0; i < hall_number; i++) {
        sale.push_back(Cinema_hall(i + 1));
    }

    initscr();
    curs_set(0);
    getmaxyx(stdscr, row, col);
    if (has_colors()) {
        start_color();
        init_pair(1, COLOR_GREEN, COLOR_BLACK); // zielony
        init_pair(2, COLOR_RED, COLOR_BLACK); // czerwony
        init_pair(3, COLOR_YELLOW, COLOR_BLACK); // zolty
    }

}
//-------------------------------------------------------------------------------------------------------------
void ncurses_rysuj() {
    //-------------------------------------Rysowanie Sal-----------------------------------------------------------
    attron(COLOR_PAIR(1));
    for(int i=0; i<hall_number; i++){
        row = 0;
        mvprintw(row,i*16, "Sala nr " );
        printw(to_string(sale[i].nr).c_str()); row++; attron(COLOR_PAIR(1));
        mvprintw(row,i*16, "-------------" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "|            |" );row++;
        mvprintw(row,i*16, "-------------" );row++;

        if(sale[i].seats==sale[i].population) {
            mvprintw(row,i*16, "Pelna sala"); row++;}
        else {
            mvprintw(row,i*16, "Osob:");
            printw(to_string(sale[i].population).c_str());row++;
        }
        if(sale[i].movie){ mvprintw(row,i*16, "Leci film"); }
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
        hh = "00";
    } else if (hours < 10) {
        hh = "0" + to_string(hours);
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

    mvprintw(40, 30, clk.c_str());
    mvprintw(41, 30, "liczba ludzi, ktorzy odwiedzili kino: ");
    printw(to_string(visitors).c_str());
}
//-------------------------------------------------------------------------------------------------------------

void time_simulation() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        minutes++;
        minutes = minutes % 60;

        if (minutes == 0) {
            hours++;
        }

        if (minutes == 10) {
            sale[0].movie = true;
        }
        if (minutes == 15) {
            sale[1].movie = true;
        }
        if (minutes == 20) {
            sale[0].end_show = true;

            sale[2].movie = true;
        }
        if (minutes == 25) {
            sale[0].end_show = false;
            sale[0].movie = false;

            sale[1].end_show = true;

            sale[3].movie = true;
        }
        if (minutes == 30) {
            sale[1].end_show = false;
            sale[1].movie = false;

            sale[2].end_show = true;

            sale[4].movie = true;
        }
        if (minutes == 35) {
            sale[2].end_show = false;
            sale[2].movie = false;

            sale[3].end_show = true;
        }
        if (minutes == 40) {
            sale[3].end_show = false;
            sale[3].movie = false;

            sale[4].end_show = true;
        }
        if (minutes == 45) {
            sale[4].end_show = false;
            sale[4].movie = false;
        }

        m_tickets.lock();

        for (int i = 0; i < hall_number; i++) {
            if (sale[i].end_show) {
                while (sale[i].population > 0) {
                    sale[i].population--;
                    visitors++;
                }
            }
        }
        m_tickets.unlock();
    }
}

void wolna_kasa() {
    while (true) {
        lock_guard<mutex> lk(m_counters);
        if (!cnt_queue.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            cv_counters.notify_one();
        }
    }
}

void sprawdzenie_biletow() {
    while (true) {
        lock_guard<mutex> lk(m_tickets);
        if (!ctrl_queue.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
            cv_tickets.notify_all();
        }
    }
}

void bilety() {
    unique_lock<mutex> lk1(m_tickets);
    cv_tickets.wait(lk1, []() { return free_ctrls > 0; });

    free_ctrls--;

    Client klient = ctrl_queue.front();                //pierwszy klient przechodzi do kasy
    ctrl_queue.pop_front();

    if (sale[klient.hall].population < sale[klient.hall].seats && !sale[klient.hall].movie) {
        sale[klient.hall].population++;                //klienci wchodza na sale jezeli nie jest zapelniona i
    } else {                                            //film sie jeszcze nie zaczal
        ctrl_queue.push_back(klient);              //klienci czekaja w kolejce przed salami
    }
    lk1.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(10));

    lk1.lock();

    free_ctrls++;
    lk1.unlock();

}


void klient() {
    unique_lock<mutex> lk(m_counters);
    cv_counters.wait(lk, []() { return free_cnts > 0; });

    Client klient = Client();
    free_cnts--;

    if (!cnt_queue.empty()) {
        klient = cnt_queue.front();
        cnt_queue.pop_front();
    }
    lk.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(klient.czas_transakcji * 10));

    lk.lock();
    free_cnts++;
    ctrl_queue.push_back(klient);

    lk.unlock();

    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    m_tickets.lock();
    t_tickets.push_back(thread(bilety));
    t_tickets.back().detach();
    m_tickets.unlock();
}


int main() {
    srand(time(NULL));
    init();                        //startujemy

    initscr();

    thread watek_wolna_kasa = thread(wolna_kasa);
    thread watek_wolne_miejsce_do_sprawdzania_biletow = thread(sprawdzenie_biletow);
    thread watek_czas = thread(time_simulation);

    for (int i = 0; i < viewers; i++) {           //budowanie kolejki klientow
        t_clients.push_back(thread(klient));
    }
    while (true) {
        if (kbhit()) {
            key = getch();
            if (key == ESC) {
                watek_wolna_kasa.join();
                watek_wolne_miejsce_do_sprawdzania_biletow.join();
                watek_czas.detach();

                for (int i = 0; i < viewers; i++) {
                    t_clients[i].join();
                }
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