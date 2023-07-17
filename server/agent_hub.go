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
type State int

const (
	StateSelect1 State = iota
	StateSelect2
	StateSay
)

type Candidate struct {
	Member int
	You    int
	State  State
}

type Say struct {
	State State
}

type Ack struct {
	Say string
}

type Select struct {
	Index int
	Say   string
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

func (a *Game) doSelect(i int, state State, say string) (next_i int, err error) {
	log.Printf("agent%d: %v is selecting", i, a.agents[i].c.Request().RemoteAddr)
	member := len(a.agents)
	err = websocket.JSON.Send(a.agents[i].c, &Candidate{Member: member, You: i, State: state})
	if err != nil {
		return
	}
	var sel Select
	err = websocket.JSON.Receive(a.agents[i].c, &sel)
	if err != nil {
		return
	}
	if sel.Say != say {
		err = fmt.Errorf("agent%d should say %s but said %s", i, say, sel.Say)
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

func (a *Game) sayNeighbor(i int, state State, say string) error {
	left, right := nearIndex(i, len(a.agents))
	var err [2]error
	var wg sync.WaitGroup
	wg.Add(2)
	doSay := func(i, erri int) {
		defer wg.Done()
		log.Printf("agent%d: %v will say %s", i, a.agents[i].c.Request().RemoteAddr, say)
		err[erri] = websocket.JSON.Send(a.agents[i].c, &Say{State: state})
		if err[erri] != nil {
			return
		}
		var ack Ack
		err[erri] = websocket.JSON.Receive(a.agents[i].c, &ack)
		if ack.Say != say {
			err[erri] = fmt.Errorf("agent%d should say %s but said %s", i, say, ack.Say)
			return
		}
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
	log.Println("play start!")
	for j := 0; j < h.phaseLimit; j++ {
		log.Printf("phase %d", j)
		var err error
		i, err = a.doSelect(i, StateSelect1, "Sec")
		if err != nil {
			log.Println(err)
			return
		}
		i, err = a.doSelect(i, StateSelect2, "Hack")
		if err != nil {
			log.Println(err)
			return
		}
		err = a.sayNeighbor(i, StateSay, "365")
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
