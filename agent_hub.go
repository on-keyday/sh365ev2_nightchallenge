package main

import (
	"errors"
	"fmt"
	"log"
	"math/rand"
	"net/http"
	"sync"
	"time"

	"golang.org/x/net/websocket"
)

type Agent struct {
	c     *websocket.Conn
	start chan struct{}
	close chan struct{}
}

type Hub struct {
	queue        chan *Agent
	agentPerGame int
	phaseLimit   int
}

type Game struct {
	agents []*Agent
}

type Hello struct {
	Count int
}

type Ready struct {
	Count    int
	Selected bool
}

type Candidate struct {
	Member int
	You    int
	Say    string
}

type Say struct {
	Say string
}

type Ack struct {
	OK bool
}

type Select struct {
	Index int
}

func (h *Hub) enqueAgent(a *Agent) {
	h.queue <- a
	handleAgent(a)
}

func handleAgent(a *Agent) {
	<-a.start
	<-a.close
}

func nearIndex(i, mod int) (int, int) {
	if i == 0 {
		return mod - 1, 1
	} else {
		return i - 1, (i + 1) % mod
	}
}

func (a *Game) doSelect(i int, say string) (next_i int, err error) {
	log.Printf("agent%d: %v is selecting", i, a.agents[i].c.Request().RemoteAddr)
	member := len(a.agents)
	err = websocket.JSON.Send(a.agents[i].c, &Candidate{Member: member, You: i, Say: say})
	if err != nil {
		return
	}
	var sel Select
	err = websocket.JSON.Receive(a.agents[i].c, &sel)
	if err != nil {
		return
	}
	if sel.Index >= len(a.agents) {
		err = fmt.Errorf("index out of range %d/%d", sel.Index, len(a.agents))
		return
	}
	if sel.Index == i {
		err = fmt.Errorf("same index %d", sel.Index)
		return
	}
	log.Printf("agent%d: %v select agent%d with saying %s", i, a.agents[i].c.Request().RemoteAddr, sel.Index, say)
	return sel.Index, nil
}

func (a *Game) sayNeighbor(i int, say string) error {
	left, right := nearIndex(i, len(a.agents))
	var err [2]error
	var wg sync.WaitGroup
	wg.Add(2)
	doSay := func(i, erri int) {
		defer wg.Done()
		log.Printf("agent%d: %v will say %s", i, a.agents[i].c.Request().RemoteAddr, say)
		err[erri] = websocket.JSON.Send(a.agents[i].c, &Say{Say: say})
		if err[erri] != nil {
			return
		}
		var ack Ack
		err[erri] = websocket.JSON.Receive(a.agents[i].c, &ack)
		log.Printf("agent%d: %v said %s", i, a.agents[i].c.Request().RemoteAddr, say)
	}
	go doSay(left, 0)
	go doSay(right, 1)
	wg.Wait()
	return errors.Join(err[:]...)
}

func (h *Hub) playGame(a *Game) {
	c := make(chan struct{})
	for i := range a.agents {
		a.agents[i].close = c
		a.agents[i].start <- struct{}{}
	}
	defer close(c)
	i := rand.Intn(len(a.agents))
	start := time.Now()
	defer func() {
		end := time.Now()
		log.Println("play time: ", end.Sub(start))
	}()
	for j := 0; j < h.phaseLimit; j++ {
		log.Printf("phase %d", j)
		next_i, err := a.doSelect(i, "Sec")
		if err != nil {
			log.Println(err)
			return
		}
		i, err = a.doSelect(next_i, "Hack")
		if err != nil {
			log.Println(err)
			return
		}
		err = a.sayNeighbor(i, "365")
		if err != nil {
			log.Println(err)
			return
		}
	}

}

func gameSpawner(h *Hub) {
	for {
		var game Game
		for i := 0; i < h.agentPerGame; i++ {
			a := <-h.queue
			log.Printf("agent%d: %v here", i, a.c.Request().RemoteAddr)
			game.agents = append(game.agents, a)
		}
		go h.playGame(&game)
	}
}

func main() {
	h := &Hub{queue: make(chan *Agent), agentPerGame: 5, phaseLimit: 10000}
	go gameSpawner(h)
	http.Handle("/", http.FileServer(http.Dir(".")))
	http.Handle("/senda", websocket.Handler(func(c *websocket.Conn) {
		agent := &Agent{c: c, start: make(chan struct{}, 1)}
		h.enqueAgent(agent)
	}))
	http.ListenAndServe(":8090", nil)
}
