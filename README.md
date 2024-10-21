# Boraciu Ionut-Sorin 325CA

* Am pornit de la rezolvarea mea a laboratorului 7.
* In implementare aceasta server-ul este cel care face tot "heavy-lifting-ul", in sensul ca acesta
trimite catre server datele prelucrate, gata de afisare. ( asa mi s-a parut cel mai logic sa fie structurat).
* Atat in server cat si in client m-am folosit de multiplexarea I/O pentru a putea sa ma aflu intr-o stare de asteptare pana cand se intampla un event pe unul dintre sockets, moment in care incepe executia.

## Server

* Server-ul asteapta sa primeasca conexiuni TCP de la clienti, moment in care trebuie sa verifice
id-ul client-ului pentru a se asigura ca numai exista alt client cu acelasi id. Trebuie sa realoce cele 3 structuri dinamice daca este cazul. Si sa verifice daca client-ul a mai fost conectat la server, pentru a ii pastra abonarile vechi
* In cazul in care server-ul primeste conexiuni UDP, acestea sunt rezervate pentru un unic socket si primeste mesajul de maxim 1551 bytes pe care il va interpreta, aseza intr-o structura si trimite catre clieti care s-au abonat la topic-ul respectiv.
    * In cazul in care sunt folosite wildcard-uri, atat elementul cautat cat si topic-urile la care este abonat respectivul client vor si puse in cate un vector de siruri de caractere ( imitand o lista de string-uri), urmand sa fie comparate cuvant cu cuvant, pentru a vedea daca putem face matching sau nu
* Daca server-ul primeste un mesaj de la unul dintre sockets conectacti prin TCP, acesta poate fi una dintre 2 comenzi: subscribe sau unsubscribe ( daca primeste alta comanda se afiseaza un mesaj de eroare informand utilizatorul de cele 2 comenzi valide)
    * Subscribe: verific ca am primit un topic valid, si il adaug la lista de topic-uri la care este abonat client-ul care a trimis mesajul. Daca topic-ul este valid trimit un mesaj corespunzator catre client care sa afiseze ca s-a abonat, altfel trimite un mesaj "not ok" pentru a afisa o eroare la client
    * Unsubscribe: ma asigur ca exista abonamentul pe care vreau sa il elimin, in caz negativ trimit un mesaj de eroare catre client, instintandu-l de acest aspect.
* STDIN: singura comanda valida este exit, orice alta comanda va produce un mesaj de eroare.
    * Exit, va trimite un semnal catre toti abonatii pentru a se opri, apoi va opri toate conexiunile sale si va elibera resursele programului.

## Client

* Client-ul nu are multe functionalitati, intrucat server-ul face cat mai mult operatiuni posibile
* Acesta poate intercepta de la STDIN unul din urmatoarele mesaje:
    * subscribe/unsubscribe: trimite un mesaj server-ului si asteapta confirmarea pentru a afisa mesajul corespunzator
    * exit: inchide socket-ul si programul
    * orice altceva: afiseaza pe ecran comenziile valide
* Daca acesta primeste prin TCP un mesaj de la server, va afisa datele primite la ecran ( date deja interpretate de server)