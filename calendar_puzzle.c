#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#define INITIAL_BOARD 0x0303010101011fff
#define ADVANCED_BOARD 0x03030101010101f1

#define BASIC_BOTTOM ((Board) 0x1fff)
#define ADVANCED_BOTTOM ((Board) 0x1)

typedef ushort PieceSmall;
typedef unsigned long long Board;

typedef struct _piece {
    char symbol;
    PieceSmall orientations[8];
    Board placements[250];
    Board solution;
    struct _piece *next;
} Piece;

void show_result(Piece *pieces);

PieceSmall basic_pieces[]={0xccc0, 0x8cc0, 0xc8c0, 0x88c8, 0x88e0, 0x888c, 0x44c8, 0xc460};
PieceSmall advanced_pieces[]={0xf000, 0x8cc0, 0xc8c0, 0xe440, 0x88e0, 
                              0x888c, 0x44c8, 0xc460, 0xc880, 0x8c40};

#define NUMBER_OF_BASIC_PIECES ((sizeof(basic_pieces)) / sizeof(PieceSmall))
#define NUMBER_OF_ADVANCED_PIECES ((sizeof(advanced_pieces)) / sizeof(PieceSmall))

/* Rotate the piece 90 degrees to the right. */
PieceSmall rotate_piece(PieceSmall piece)
{
    PieceSmall rotated=0;
    PieceSmall mask = 0x8888;
    PieceSmall y=0;
    int    s=0;

    for (s=3; s >= 0; s--)
    {
        /* Rotate by taking the first column and making it the first row,
         * then the second column and make that the second row, etc.
         */
        y = (piece & mask) >> s;
        y = ((y & 1) << 3) | 
            ((y & 0x10) >> 2) | 
            ((y & 0x100) >> 7) | 
            ((y & 0x1000) >> 12);
        rotated |= y << (4 * s);
        mask = mask >> 1;
    }

    return rotated;
}

/* Flip the piece around the Y-axis. */
PieceSmall flip_piece(PieceSmall piece)
{
    PieceSmall flipped=0;
    PieceSmall a,b,c,d;
    PieceSmall mask=0x8888;

    a = mask & piece;
    b = (mask >> 1) & piece;
    c = (mask >> 2) & piece;
    d = (mask >> 3) & piece;

    flipped = (d << 3) | (c << 1) | (b >> 1) | (a >> 3);

    return flipped;
}

/* Move the piece to the top left corner. */
PieceSmall normalize_piece(PieceSmall piece)
{
    PieceSmall mask=0xf000;

    if (piece != 0)
    {
        /* Move it to the top.*/
        while ((mask & piece) == 0)
        {
            piece = piece << 4;
        }

        /* Move it to the left. */ 
        mask = 0x8888;
        while ((mask & piece) == 0)
        {
            piece = piece << 1;
        }
    }

    return piece;
}

/* Debug - Show a simple piece. */
void print_piece(PieceSmall piece)
{
    PieceSmall mask=0x8000;
    int i;

    printf("Piece: %04x\n", piece);
    for (i=0; i<sizeof(piece)*8; i++)
    {
        printf("%c", (piece&mask) ? 'X': ' ');
        mask = mask >> 1;
        if (((i+1)%4) == 0)
        {
            printf("\n");
        } 
    }    
    printf("\n");
}

/* Debug - Show the board. */
void print_board(Board board, char sym)
{
    Board mask=0x8000000000000000;
    int i;


    for (i=0; i<sizeof(board)*8; i++)
    {
        printf("%c", (board&mask) ? sym : '.');
        mask = mask >> 1;
        if (((i+1)%8) == 0)
        {
            printf("\n");
        } 
    }    
    printf("\n");
}

/* Add an orientation for a piece, skipping existing matching orientations. */
void add_orientations(Piece *piece, PieceSmall orientation)
{
    int i;

    for (i=0; i < 8; i++)
    {
        if (piece->orientations[i] == 0)
        {
            piece->orientations[i] = orientation;
            return;
        }
        if (piece->orientations[i] == orientation)
        {
            /* Already in list, skip it. */
            return;
        }
    }
}

/* Create an entry for each piece including all of the possible orientations. */
Piece *build_pieces(PieceSmall *base_pieces, int num_of_pieces)
{
    PieceSmall next_orientation=0;
    Piece  *last_piece = NULL;
    Piece  *new_piece = NULL;
    Piece  *result = NULL;
    int    i;
    int    j;

    for (i=0; i < num_of_pieces; i++)
    {
        new_piece = calloc(1, sizeof(Piece));

        new_piece->symbol = 'A' + i;
        new_piece->solution = (Board) 0;
        if (last_piece != NULL)
        {
            last_piece->next = new_piece;
        }
        else
        {
            result = new_piece;
        }
        last_piece = new_piece;

        next_orientation = base_pieces[i];
        add_orientations(new_piece, next_orientation);

        for(j=0; j<3; j++)
        {
            next_orientation = rotate_piece(next_orientation);
            next_orientation = normalize_piece(next_orientation);
            add_orientations(new_piece, next_orientation);
        }

        next_orientation = flip_piece(base_pieces[i]);
        next_orientation = normalize_piece(next_orientation);
        add_orientations(new_piece, next_orientation);

        for(j=0; j<3; j++)
        {
            next_orientation = rotate_piece(next_orientation);
            next_orientation = normalize_piece(next_orientation);
            add_orientations(new_piece, next_orientation);
        }
    }
    return result;
}

/* Transpose a simple piece and put it on the top left corner of the board.*/
Board piece_placement(PieceSmall p)
{
    Board result=(Board)0;

    result = ((Board) p & 0xf000) << 48;
    result |= ((Board) p & 0xf00) << 44;
    result |= ((Board) p & 0xf0) << 40;
    result |= ((Board) p & 0xf) << 36;

    return result;
}

/* Determine all of the places that the pieces can be placed. */
void add_placements(Board board, Piece *pieces, Board bottom)
{
    Piece *current;
    Board placement;
    int count;
    int base;

    current = pieces;
    while (current != NULL)
    {
        count = 0;
        base = 0;
        while ((base < 8) && (current->orientations[base] != 0))
        {
            placement = piece_placement(current->orientations[base]);
            while (((placement & bottom) == (Board) 0))
            {
                if ((placement & board) == (Board) 0)
                {
                    current->placements[count++] = placement;
                }
                placement = placement >> 1;
            }
            base++;
        }
        current = current->next;
    }
}

/* Mark off the month and day on the board. */
Board add_date(Board board, int month, int day, int weekday)
{
    int row;
    int col;

    row = (month - 1) / 6;
    col = (month - 1) % 6;
    board |= ((Board) 1) << (64 - (8*row) - col - 1);

    row = (day - 1) / 7;
    col = (day - 1) % 7;
    board |= ((Board) 1) << (48 - (8*row) - col - 1 );

    if (weekday >= 0)
    {
        row = (weekday) / 4;
        col = (weekday ) % 4;
        board |= ((Board) 1) << (12 - (9*row) - col);
    }

    return board;
}

/* Recursive routine to solve the puzzle. */
int solve(Board board, Piece *pieces)
{
    Piece *this_piece = pieces;
    Piece *remaining_pieces = this_piece->next;
    int index;
    int result=0;

    if (NULL == this_piece)
    {
        return 0;
    }
    remaining_pieces = this_piece->next;

    index = 0;
    while ((!result) && (this_piece->placements[index]))
    {
        if (! (this_piece->placements[index] & board)) 
        {
            this_piece->solution = this_piece->placements[index];
            if (NULL == remaining_pieces)
            {
                result = 1;
            }
            else
            {
                result = solve((board ^ this_piece->placements[index]), remaining_pieces);
            }
        }
        index++;
    }
    return result;
}

/* Add the solution for a single piece to the general solution. */
void add_solution(char ans[8][8], Piece *p)
{
    int i, j;
    Board mask=(Board) 0x8000000000000000;

    for (i=0; i<8; i++)
    {
        for (j=0; j<8; j++)
        {
            if (mask & p->solution)
            {
                ans[i][j] = p->symbol;
            }
            mask = mask >> 1;
        }
    }
}

/* Print the solution. */
void show_result(Piece *pieces)
{
    Piece *p=pieces;
    char answer[8][8];
    int i, j;

    memset(answer, ' ', sizeof(answer));

    while (NULL != p)
    {
        add_solution(answer, p);
        p = p->next;
    }
    printf("==========\n");
    for (i=0; i<8; i++)
    {
        for (j=0; j<8; j++)
        {
            printf(" %c ", answer[i][j]);
        }
        printf("\n");
    }
}

#define SOLVE

int main(int argc, char **argv)
{
    int opt;
    int day=0;
    int month=0;
    int weekday=-1;
    Board board=INITIAL_BOARD;
    Board bottom=BASIC_BOTTOM;
    PieceSmall *piece_set=basic_pieces;
    int num_of_pieces=NUMBER_OF_BASIC_PIECES;
    Piece *piece_list;
    int solved=0;
    int advanced=0;

    while ((opt = getopt(argc, argv, "ad:m:w:")) != -1)
    {
        switch (opt)
        {
            case 'a':
                advanced = 1;
                break;
            case 'd':
                day = atoi(optarg);
                break;
            case 'm':
                month = atoi(optarg);
                break;
            case 'w':
                weekday = atoi(optarg);
                advanced = 1;
                break;
        }
    }

    if (advanced)
    {
        num_of_pieces = NUMBER_OF_ADVANCED_PIECES;
        piece_set = advanced_pieces;
        board = ADVANCED_BOARD;
        bottom = ADVANCED_BOTTOM;        
    }

    piece_list = build_pieces(piece_set, num_of_pieces);
    printf("Month = %d, Day = %d\n", month, day);
    board = add_date(board, month, day, weekday);
    add_placements(board, piece_list, bottom);
#ifdef SOLVE
    solved = solve(board, piece_list);
    if (solved)
    {
        show_result(piece_list);
    }
    else
    {
        printf("Failed to solve puzzle for %d/%d\n", month, day);
    }
#endif
}