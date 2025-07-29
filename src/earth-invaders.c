#include "wasm4.h"
#include <stdlib.h>


/* ====== CONSTANTES ====== */

// Limites de objetos simultâneos
#define MAX_TIROS 6
#define MAX_TIROS_INIMIGOS 7
#define NUM_ESTRELAS 20

// Dimensões da tela (160x160 pixels) e sprites (32x32 pixels)
#define SCREEN_SIZE 160
#define SPRITE_TOTAL_SIZE 32

// Hitbox da nave do jogador (área colisível menor que o sprite)
#define PLAYER_HITBOX_WIDTH 17
#define PLAYER_HITBOX_HEIGHT 10
#define PLAYER_OFFSET_X 3
#define PLAYER_OFFSET_Y 2

// Hitbox da nave inimiga (área colisível menor que o sprite)
#define ENEMY_HITBOX_WIDTH 14
#define ENEMY_HITBOX_HEIGHT 15
#define ENEMY_OFFSET_X 9
#define ENEMY_OFFSET_Y 6

// Hitbox da nave boss (área colisível menor que o sprite)
#define BOSS_HITBOX_WIDTH 16
#define BOSS_HITBOX_HEIGHT 16
#define BOSS_OFFSET_X 7
#define BOSS_OFFSET_Y 4


/* ====== VARIÁVEIS GLOBAIS ====== */
int nave_x = (SCREEN_SIZE / 2) - (PLAYER_OFFSET_X + PLAYER_HITBOX_WIDTH / 2);   // Posição inicial x da nave do jogador
int nave_y = (SCREEN_SIZE / 2) - (PLAYER_OFFSET_Y + PLAYER_HITBOX_HEIGHT / 2) - 8;   // Posição inicial y da nave do jogador
int contador_frames = 0;   // Contador geral de frames para eventos temporizados
int pontuacao = 0;   // Score do jogador
uint8_t gamepad_anterior = 0;   // Armazena o estado anterior do gamepad
int vida_jogador = 3;
int tela = 0;
int primeiro_frame_jogo = 1; // 1 = true, 0 = false (para evitar o double click no primeiro frame)
int delay_inimigos = 90; // ~1.5 segundos para spawnar inimigos


/* ====== ESTRUTURA DO INIMIGO ====== */
int max_inimigos_ativos;

typedef struct {
    int x, y;     // Posição
    int ativo;    // Flag que indica se ele está ativo (1) ou não (0)
    int tiro_cooldown; // cooldown para o próximo tiro
} Inimigo;

Inimigo inimigos[10];
int inimigo_spawn_timer = 0;

void spawn_inimigos() {
    int ativos = 0;

    // Conta quantos inimigos já estão ativos
    for (int j = 0; j < max_inimigos_ativos; j++) {
        if (inimigos[j].ativo) ativos++;
    }

    // Só spawna um novo inimigo se houver espaço de acordo com a dificuldade
    if (ativos < max_inimigos_ativos) {
        for (int i = 0; i < max_inimigos_ativos; i++) {
            if (!inimigos[i].ativo) {
                inimigos[i].ativo = 1;
                inimigos[i].x = rand() % (SCREEN_SIZE - SPRITE_TOTAL_SIZE);
                inimigos[i].y = -SPRITE_TOTAL_SIZE;
                inimigos[i].tiro_cooldown = 0;
                break;
            }
        }
    }
}


/* ====== ESTRUTURA DO BOSS ======*/
typedef struct {
    int x, y;
    int ativo;
    int vida;
} InimigoBoss;

InimigoBoss boss = {0, 0, 0, 0};

void spawn_boss() {
    if (!boss.ativo) {
        boss.ativo = 1;
        boss.vida = 3;
        boss.x = rand() % (SCREEN_SIZE - SPRITE_TOTAL_SIZE);
        boss.y = -SPRITE_TOTAL_SIZE;
    }
}


/* ====== SISTEMA DE TIROS ====== */
int tiro_cooldown = 0;   // Timer entre disparos para saber quando podemos atirar

typedef struct {
    int x, y;    // Posição
    int ativo;   // Flag que indica se ele está ativo (1) ou não (0)
} Tiro;

Tiro tiros[MAX_TIROS];
Tiro tiros_inimigos[MAX_TIROS_INIMIGOS];

void disparar_tiro_jogador(int x, int y) {
    for (int i = 0; i < MAX_TIROS; i++) {
        if (!tiros[i].ativo) {
            tiros[i].x = x + PLAYER_OFFSET_X + (PLAYER_HITBOX_WIDTH/2);  // Centro da hitbox da nave
            tiros[i].y = y + PLAYER_OFFSET_Y - 2;  // Acima da hitbox
            tiros[i].ativo = 1;
            tone(600, 5, 50, TONE_PULSE2);
            break;
        }
    }
}

void disparar_tiro_inimigo(int x, int y) {
    for (int i = 0; i < MAX_TIROS_INIMIGOS; i++) {
        if (!tiros_inimigos[i].ativo) {
            tiros_inimigos[i].x = x + ENEMY_OFFSET_X + (ENEMY_HITBOX_WIDTH/2);   //Centraliza no hitbox
            tiros_inimigos[i].y = y + ENEMY_OFFSET_Y + ENEMY_HITBOX_HEIGHT;   //Parte inferior do hitbox
            tiros_inimigos[i].ativo = 1;
            tone(300, 5, 35, TONE_PULSE1);
            break;
        }
    }
}


/* ====== ESTRUTURA DA ESTRELA ======*/
int estrelas_inicializadas = 0;

typedef struct {
    int x, y;
} Estrela;

Estrela estrelas[NUM_ESTRELAS];


/* ====== ULT ======*/
int explosao_ativa = 0;
int explosao_timer = 0;
int explosao_cooldown = 0;


// Função auxiliar para converter int para string (mostrar pontuação)
void int_to_str(int num, char* buffer) {
    int i = 0;
    if (num == 0) {
        buffer[i++] = '0';
    } else {
        while (num > 0) {
            buffer[i++] = '0' + (num % 10);
            num /= 10;
        }
    }
    buffer[i] = '\0';

    // Inverte a string
    for (int j = 0; j < i / 2; j++) {
        char tmp = buffer[j];
        buffer[j] = buffer[i - j - 1];
        buffer[i - j - 1] = tmp;
    }
}


// Atualiza a dificuldade baseada na pontuação
int tempo_entre_spawns = 0;

void atualizar_dificuldade() {

    // Extremo
    if (pontuacao >= 60) {
        tempo_entre_spawns = 40;
        max_inimigos_ativos = 4;
    }

    // Difícil
    else if (pontuacao >= 30) {
        tempo_entre_spawns = 40;
        max_inimigos_ativos = 3;

    // Médio
    } else if (pontuacao >= 5) {
        tempo_entre_spawns = 40;
        max_inimigos_ativos = 2;

    // Fácil
    } else {
        tempo_entre_spawns = 1;
        max_inimigos_ativos = 1;
    }
}


/* ====== SPRITES ====== */

const uint8_t nave[] = {
    0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xfe,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xd5,0x5f,0xff,0xff,0xff,0xff,0xff,0xff,0x7b,0xe7,0xff,0xff,0xff,0xff,0xff,0xf5,0xfa,0xad,0x7f,0xff,0xff,0xff,0xff,0x57,0xf9,0x9f,0x57,0xff,0xff,0xff,0xfd,0xa9,0xfa,0xad,0xa9,0xff,0xff,0xff,0xfd,0xa2,0x55,0x56,0x29,0xff,0xff,0xff,0xff,0x6a,0x8a,0x8a,0xa7,0xff,0xff,0xff,0xff,0xd6,0xaa,0xaa,0x5f,0xff,0xff,0xff,0xff,0xfd,0x55,0x55,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff,0xff
};

const uint8_t nave_inimiga[] = {
    0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x8a,0xaa,0x2a,0xa8,0xaa,0xaa,0xaa,0xaa,0x82,0x96,0x25,0xa0,0xaa,0xaa,0xaa,0xaa,0x80,0x94,0x05,0x80,0xaa,0xaa,0xaa,0xaa,0x80,0x04,0x04,0x00,0xaa,0xaa,0xaa,0xaa,0x88,0x00,0x00,0x08,0xaa,0xaa,0xaa,0xaa,0x8a,0x00,0x00,0x28,0xaa,0xaa,0xaa,0xaa,0x9a,0x41,0x10,0x69,0xaa,0xaa,0xaa,0xaa,0x9a,0x10,0x41,0x29,0xaa,0xaa,0xaa,0xaa,0xaa,0x20,0x42,0x2a,0xaa,0xaa,0xaa,0xaa,0xaa,0x68,0x0a,0x6a,0xaa,0xaa,0xaa,0xaa,0xaa,0x68,0x0a,0x6a,0xaa,0xaa,0xaa,0xaa,0xaa,0xa8,0x0a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xa8,0x0a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x2a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x2a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x6a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
};

const uint8_t nave_inimiga_boss[] = {
    0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xa8,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xa1,0x2a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xa1,0x2a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x85,0x4a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x15,0x52,0xaa,0xaa,0xaa,0xaa,0xaa,0xa8,0x55,0x54,0xaa,0xaa,0xaa,0xaa,0xaa,0xa1,0x54,0x55,0x2a,0xaa,0xaa,0xaa,0xaa,0x05,0x50,0x15,0x42,0xaa,0xaa,0xaa,0xa8,0x55,0x40,0x05,0x54,0xaa,0xaa,0xaa,0xaa,0x05,0x50,0x15,0x42,0xaa,0xaa,0xaa,0xaa,0xa1,0x54,0x55,0x2a,0xaa,0xaa,0xaa,0xaa,0xa8,0x55,0x54,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x15,0x52,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0x85,0x4a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xa1,0x2a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xa1,0x2a,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xa8,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
};

const uint8_t heart[] = {
    0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaf,0xab,0xea,0xaa,0xaa,0xaa,0xaa,0xaa,0xb5,0xed,0x7a,0xaa,0xaa,0xaa,0xaa,0xaa,0xd0,0x75,0x5e,0xaa,0xaa,0xaa,0xaa,0xaa,0xd1,0x55,0x5e,0xaa,0xaa,0xaa,0xaa,0xaa,0xb5,0x55,0x7a,0xaa,0xaa,0xaa,0xaa,0xaa,0xad,0x55,0xea,0xaa,0xaa,0xaa,0xaa,0xaa,0xab,0x57,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xde,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xba,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa,0xaa
};


/* ====== MAIN ====== */
void update () {

    PALETTE[0] = 0x000000; // 1 - Preto
    PALETTE[1] = 0x6CEDED; // 2 - Azul elétrico
    PALETTE[2] = 0x2d879a; // 3 - Azul
    PALETTE[3] = 0x404040; // 4 - Cinza Escuro

    uint8_t gamepad = *GAMEPAD1;    // Leitura do controle

    // Inicializa as estrelas em uma posição fixa para o menu, e aleatórias para as outras
    if (!estrelas_inicializadas) {
        if (tela == 0 || tela == 1) {
            Estrela fixas[NUM_ESTRELAS] = {
                {10, 20}, {50, 40}, {90, 15}, {130, 30}, {30, 80},
                {70, 60}, {110, 90}, {150, 70}, {20, 120}, {60, 110},
                {100, 140}, {140, 130}, {40, 30}, {80, 50}, {120, 10},
                {160, 20}, {25, 150}, {75, 130}, {115, 80}, {155, 100}
            };
            for (int i = 0; i < NUM_ESTRELAS; i++) {
                estrelas[i] = fixas[i];
            }
        } else {
            srand((unsigned int)(gamepad + contador_frames));
            for (int i = 0; i < NUM_ESTRELAS; i++) {
                estrelas[i].x = rand() % SCREEN_SIZE;
                estrelas[i].y = rand() % SCREEN_SIZE;
            }
        }

        estrelas_inicializadas = 1;
    }


    /* ====== TELA MENU ====== */
    if (tela == 0) {

        // Desenha as estrelas estáticas
        *DRAW_COLORS = 4;
        for (int i = 0; i < NUM_ESTRELAS; i++) {
            rect(estrelas[i].x, estrelas[i].y, 1, 1);
        }

        *DRAW_COLORS = 2;
        text("Earth Invaders", 25, 15);
        text("Press X to start", 16, 85);

        *DRAW_COLORS = 3;
        text("\x84 \x85 \x86 \x87: Move", 10, 120);
        text("\x80: Shoot", 10, 130);
        text("\x81: Ult", 10, 140);

        *DRAW_COLORS = 0x0324;
        blit(nave, nave_x, nave_y, SPRITE_TOTAL_SIZE, SPRITE_TOTAL_SIZE, BLIT_2BPP);

        // Pressionar X para ir na próxima tela
        if (gamepad & BUTTON_1) {
            tone(200, 10, 50, TONE_PULSE1);
            contador_frames = 0;    // Reseta o contador para evitar duplo clique
            tela = 1;
        }
    }

    /* ====== TELA HISTÓRIA ====== */
    else if (tela == 1) {

        // Desenha as estrelas estáticas
        *DRAW_COLORS = 4;
        for (int i = 0; i < NUM_ESTRELAS; i++) {
            rect(estrelas[i].x, estrelas[i].y, 1, 1);
        }

        contador_frames++;

        *DRAW_COLORS = 2;
        text("The year is 2552.", 8, 5);
        
        text("Humans from Earth", 10, 25);
        text("are invading your", 10, 35);
        text("home planet. They", 10, 45);
        text("seek to exploit", 10, 55);
        text("its resources and", 10, 65);
        text("enslave your kind.", 10, 75);

        text("You are the last", 10, 95);
        text("line of defense.", 10, 105);

        text("Don't let them pass.", 2, 125);

        *DRAW_COLORS = 3;
        text("Press X to begin", 13, 143);

        // Vai para o jogo (só permite depois de 30 frames, pra evitar apertar 2x sem querer)
        if (contador_frames > 30 && (gamepad & BUTTON_1) && !(gamepad_anterior & BUTTON_1)) {
            tone(400, 10, 50, TONE_PULSE2);
            contador_frames = 0;
            tela = 2;
        }

    }

    /* ====== TELA JOGO ====== */
    else if (tela == 2) {
        atualizar_dificuldade();          // Ajusta a dificuldade conforme a pontuação
        contador_frames++;                // Contador de frames para temporização

        // Delay inicial para evitar double click quando chega nessa tela
        if (primeiro_frame_jogo) {
            primeiro_frame_jogo = 0; // Marca como não é mais o primeiro frame
            tiro_cooldown = 15;
        }

        // Background animado de estrelas
        *DRAW_COLORS = 4;
        for (int i = 0; i < NUM_ESTRELAS; i++) {
            estrelas[i].y += 1;  // Desce 1 pixel por vez
            if (estrelas[i].y >= SCREEN_SIZE) {
                estrelas[i].y = 0;
                estrelas[i].x = rand() % SCREEN_SIZE;
            }
            rect(estrelas[i].x, estrelas[i].y, 1, 1);
        }

        // Movimento da nave com as setas
        if (gamepad & BUTTON_LEFT)  nave_x -= 2;
        if (gamepad & BUTTON_RIGHT) nave_x += 2;
        if (gamepad & BUTTON_UP)    nave_y -= 2;
        if (gamepad & BUTTON_DOWN)  nave_y += 2;

        /* ====== Limites da Tela ====== */
        if (nave_x < -PLAYER_OFFSET_X)  //Esquerda
            nave_x = -PLAYER_OFFSET_X;

        if (nave_x > SCREEN_SIZE - (PLAYER_OFFSET_X + PLAYER_HITBOX_WIDTH)) //Direita
            nave_x = SCREEN_SIZE - (PLAYER_OFFSET_X + PLAYER_HITBOX_WIDTH);

        if (nave_y < -PLAYER_OFFSET_Y) // Cima
            nave_y = -PLAYER_OFFSET_Y;

        if (nave_y > SCREEN_SIZE - (PLAYER_OFFSET_Y + PLAYER_HITBOX_HEIGHT) + 5) //Baixo
            nave_y = SCREEN_SIZE - (PLAYER_OFFSET_Y + PLAYER_HITBOX_HEIGHT) + 5;
        
        // Delay para os inimigos aparecerem quando acabamos de sair da tela 1
        if (delay_inimigos > 0) {
            delay_inimigos--;
        } else {
            // Spawn de inimigos a cada 35 frames
            inimigo_spawn_timer++;
            if (inimigo_spawn_timer >= tempo_entre_spawns) {
                spawn_inimigos();
                inimigo_spawn_timer = 0;
            }
        }

        // Movimenta e desenha os inimigos se estiver ativo
        for (int i = 0; i < max_inimigos_ativos; i++) {
            if (inimigos[i].ativo) {
                inimigos[i].y += 1;  // Inimigo desce 1 pixel por frame

                if (inimigos[i].y > SCREEN_SIZE) {  // Saiu da tela
                    inimigos[i].ativo = 0;
                    vida_jogador--; // Perde uma vida

                    if (vida_jogador <= 0) {
                        contador_frames = 0;
                        tone(150, 5, 50, TONE_PULSE1);
                        tela = 3; // Game over :(
                    }
                    tone(120, 5, 50, TONE_NOISE);

                } else {
                    *DRAW_COLORS = 0x0023;
                    blit(nave_inimiga, inimigos[i].x, inimigos[i].y, SPRITE_TOTAL_SIZE, SPRITE_TOTAL_SIZE, BLIT_2BPP);
                    
                }
            }
        }

        // Inimigo atira com chances aleatórias
        for (int i = 0; i < max_inimigos_ativos; i++) {
            if (inimigos[i].ativo && inimigos[i].y < 70) {
                if (inimigos[i].tiro_cooldown > 0) {
                    inimigos[i].tiro_cooldown--;
                } else if ((rand() % 150) < 1) { // 0.75% de chance por frame
                    disparar_tiro_inimigo(inimigos[i].x, inimigos[i].y);
                    inimigos[i].tiro_cooldown = 90; // espera 60 frames (1s) para o próximo tiro
                }
            }
        }

        // A cada 2 segundos, 30% de chance de spawnar o boss
        if (contador_frames % 120 == 0) {
            if ((rand() % 100) < 30) {
                spawn_boss();
            }
        }

        // Movimenta e desenha o boss se ele estiver ativo
        if (boss.ativo) {
            if (contador_frames % 2 == 0) {
                boss.y += 1;    // Se movimenta 1 pixel a cada 2 frames
            }

            if (boss.y > SCREEN_SIZE) {
                contador_frames = 0;
                tone(150, 5, 50, TONE_PULSE1);
                tela = 3;   // Game over
            } else {
                *DRAW_COLORS = 0x0032;
                blit(nave_inimiga_boss, boss.x, boss.y, SPRITE_TOTAL_SIZE, SPRITE_TOTAL_SIZE, BLIT_2BPP);
            }
        }
        
        // Atira se apertar X (o cooldown é para evitar que o jogador dispare continuamente)
        if ((gamepad & BUTTON_1) && tiro_cooldown == 0) {
            disparar_tiro_jogador(nave_x, nave_y);
            tiro_cooldown = 15;  // Reinicia o cooldown
        }
        if (tiro_cooldown > 0) {
            tiro_cooldown--;
        }

        // Ult (explosão) se apertar Z
        if ((gamepad & BUTTON_2) && explosao_cooldown == 0 && !explosao_ativa) {
            explosao_ativa = 1;
            explosao_timer = 15;   // Tempo que a explosão demora
            explosao_cooldown = 480;   // Cooldown pra ult

            // Limpa todos os inimigos
            for (int i = 0; i < max_inimigos_ativos; i++) {
                if (inimigos[i].ativo) {
                    inimigos[i].ativo = 0;
                    pontuacao++;
                }
            }

            // Limpa todos os tiros inimigos
            for (int i = 0; i < MAX_TIROS_INIMIGOS; i++) {
                tiros_inimigos[i].ativo = 0;
            }

            // Limpa todos os tiros do jogador
            for (int i = 0; i < MAX_TIROS; i++) {
                tiros[i].ativo = 0;
            }

            // Limpa o boss
            if (boss.ativo) {
                boss.ativo = 0;
            }

            tone(400, 5, 70, TONE_PULSE1);
            tone(60, 30, 90, TONE_NOISE);
        }
        if (explosao_cooldown > 0) {
            explosao_cooldown--;
        }

        // Efeito visual da explosão (centraliza na nave, e cobre o tamanho da tela + 100 para garantir que, mesmo se a nave esteja em um dos cantos, toda a tela seja coberta)
        if (explosao_ativa) {
            int centro_x = nave_x + 16;
            int centro_y = nave_y + 16;
            int raio = (SCREEN_SIZE + 100) * (15 - explosao_timer) / 15;
            uint32_t diametro = (uint32_t)(raio * 2);
            *DRAW_COLORS = 2;
            oval(centro_x - raio, centro_y - raio, diametro, diametro);
            
            explosao_timer--;
            if (explosao_timer <= 0) {
                explosao_ativa = 0;
            }
        }

        // Mostra barra de recarga da ult
        if (explosao_cooldown > 0) {
            int largura_total = 40;
            int largura_barra = largura_total * (480 - explosao_cooldown) / 480;

            // Contorno da barra (azul escuro)
            *DRAW_COLORS = 3;
            rect(3, 17, (uint32_t)largura_total, 4);

            // Parte preenchida (azul claro)
            *DRAW_COLORS = 2;
            rect(3, 17, (uint32_t)largura_barra, 4);
        }

        // Mostra o score
        *DRAW_COLORS = 2;
        char texto[20] = "Score: ";
        char num_str[10];
        int_to_str(pontuacao, num_str);
        int i = 7;
        for (int j = 0; num_str[j] != '\0'; j++) {
            texto[i++] = num_str[j];
         }
        texto[i] = '\0';
        text(texto, 70, 5);

        //Mostra a vida
        for (int i = 0; i < vida_jogador; i++) {
            *DRAW_COLORS = 0x0023;
            blit(heart, i*13, -5, SPRITE_TOTAL_SIZE, SPRITE_TOTAL_SIZE, BLIT_2BPP);
        }

        // Se a explosão não estiver ativa...
        if (!explosao_ativa) {

            // Desenha a nave do jogador
            *DRAW_COLORS = 0x0324;
            blit(nave, nave_x, nave_y, SPRITE_TOTAL_SIZE, SPRITE_TOTAL_SIZE, BLIT_2BPP);

            // Atualiza e desenha tiros (sobe 3 pixels por frame)
            *DRAW_COLORS = 3;
            for (int i = 0; i < MAX_TIROS; i++) {

                if (tiros[i].ativo) {
                    tiros[i].y -= 2;

                    if (tiros[i].y < 0) {
                        tiros[i].ativo = 0;
                    } else {

                        // Checa colisão com os inimigos
                        for (int j = 0; j < max_inimigos_ativos; j++) {
                            if (inimigos[j].ativo && 
                                tiros[i].x >= inimigos[j].x + ENEMY_OFFSET_X && 
                                tiros[i].x <= inimigos[j].x + ENEMY_OFFSET_X + ENEMY_HITBOX_WIDTH &&
                                tiros[i].y >= inimigos[j].y + ENEMY_OFFSET_Y && 
                                tiros[i].y <= inimigos[j].y + ENEMY_OFFSET_Y + ENEMY_HITBOX_HEIGHT) {
                                
                                tiros[i].ativo = 0;
                                inimigos[j].ativo = 0;
                                pontuacao++;
                                tone(100, 5, 50, TONE_NOISE);
                            }
                        }
                        
                        // Desenha o tiro
                        if (tiros[i].ativo) {
                            rect(tiros[i].x, tiros[i].y, 1, 1); // Tiro é 1 pixel
                        }

                        // Checa colisão com o inimigo boss
                        if (boss.ativo) {
                            if (boss.ativo && 
                                tiros[i].x >= boss.x + BOSS_OFFSET_X &&
                                tiros[i].x <= boss.x + BOSS_OFFSET_X + BOSS_HITBOX_WIDTH &&
                                tiros[i].y >= boss.y + BOSS_OFFSET_Y &&
                                tiros[i].y <= boss.y + BOSS_OFFSET_Y + BOSS_HITBOX_HEIGHT) {
                                tiros[i].ativo = 0;
                                boss.vida--;
                                tone(100, 5, 50, TONE_NOISE);

                                if (boss.vida <= 0) {
                                    boss.ativo = 0;
                                    pontuacao++;
                                    tone(300, 15, 35, TONE_NOISE);
                                }
                            }
                        }
                    }
                }
            }

            // Atualiza e desenha tiros inimigos
            for (int i = 0; i < MAX_TIROS_INIMIGOS; i++) {

                if (tiros_inimigos[i].ativo) {
                    tiros_inimigos[i].y += 2;

                    //Jogador leva tiro (perde vida)
                    if (tiros_inimigos[i].x >= nave_x + PLAYER_OFFSET_X && 
                        tiros_inimigos[i].x <= nave_x + PLAYER_OFFSET_X + PLAYER_HITBOX_WIDTH &&
                        tiros_inimigos[i].y >= nave_y + PLAYER_OFFSET_Y && 
                        tiros_inimigos[i].y <= nave_y + PLAYER_OFFSET_Y + PLAYER_HITBOX_HEIGHT) {
                        tiros_inimigos[i].ativo = 0;
                        vida_jogador--;

                        if (vida_jogador <= 0) {
                            contador_frames = 0;
                            tone(150, 5, 50, TONE_PULSE1);
                            tela = 3; // Game Over
                        } else {
                            tone(120, 5, 50, TONE_NOISE);
                        }
                    }

                    if (tiros_inimigos[i].y > SCREEN_SIZE) { // Saiu da tela
                        tiros_inimigos[i].ativo = 0;
                    } else {
                        *DRAW_COLORS = 2;
                        rect(tiros_inimigos[i].x, tiros_inimigos[i].y, 1, 3);
                    }
                }
            }

        }
    }

    /* ====== TELA GAME OVER ====== */
    else if (tela == 3) {
        contador_frames++;

        // Desenha o background de estrelas de fundo estáticas
        *DRAW_COLORS = 4;
        for (int i = 0; i < NUM_ESTRELAS; i++) {
            rect(estrelas[i].x, estrelas[i].y, 1, 1);
        }

        // Centraliza "GAME OVER" (8 caracteres)
        *DRAW_COLORS = 2;
        text("Game Over", 44, 45);  // Cada caractere tem 8px de largura


        // Centraliza o score dinamicamente (de acordo com a quantidade de dígitos)
        char score_text[12] = "Score: ";
        int score_len = 7;  // "Score: " tem 7 caracteres
        int temp_score = pontuacao;
        int digits = 0;
        char num_str[4] = {0};
        
        if (temp_score == 0) {
            num_str[0] = '0';
            digits = 1;
        } else {
            while (temp_score > 0) {
                num_str[digits++] = '0' + (temp_score % 10);
                temp_score /= 10;
            }
            // Inverte os dígitos
            for (int i = 0; i < digits/2; i++) {
                char temp = num_str[i];
                num_str[i] = num_str[digits-1-i];
                num_str[digits-1-i] = temp;
            }
        }
        
        // Concatena manualmente
        for (int i = 0; i < digits; i++) {
            score_text[score_len++] = num_str[i];
        }
        score_text[score_len] = '\0';

        // Calcula posição X (considerando 8px por caractere)
        int text_width = score_len * 8;
        int pos_x = (SCREEN_SIZE - text_width) / 2;
        *DRAW_COLORS = 2;
        text(score_text, pos_x, 65);
        
        *DRAW_COLORS = 3;
        text("Press X to retry", 17, 105);

        // Se apertar X, resetar o estado e voltar para o jogo
        if ((gamepad & BUTTON_1) && !(gamepad_anterior & BUTTON_1) && contador_frames > 30) {
            nave_x = 64;
            nave_y = 53;
            pontuacao = 0;
            contador_frames = 0;
            explosao_ativa = 0;
            explosao_timer = 0;
            explosao_cooldown = 0;
            tiro_cooldown = 0;
            vida_jogador = 3;
            for (int i = 0; i < MAX_TIROS; i++) tiros[i].ativo = 0;
            for (int i = 0; i < max_inimigos_ativos; i++) inimigos[i].ativo = 0;
            for (int i = 0; i < MAX_TIROS_INIMIGOS; i++) tiros_inimigos[i].ativo = 0;
            boss.ativo = 0;
            boss.x = 0;
            boss.y = 0;
            boss.vida = 0;
            contador_frames = 0;
            tela = 2;
        }
    }
}
