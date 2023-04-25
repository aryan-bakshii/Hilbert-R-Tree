#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <stdbool.h>
// EVERY NODE HAS BETWEEN m and M entries and children unless it is root
#define M 4
#define m 2
#define MAX_CHILDREN M // M = 4; MAXIMUM NUMBER OF CHILDREN
#define MIN_CHILDREN m
#define MAX_POINTS 17
int CURRENT_ID = 0;
int num_results = 0;
bool root_split = false;
int numberOfElements = 0;
typedef struct Point Point;
struct Point
{
    int x, y;
};
typedef struct Rectangle Rectangle;
struct Rectangle
{
    Point bottom_left;
    Point top_right; // coordinates of the rectangle
    int h;           // Hilbert value of the rectangle center
};
typedef struct LeafEntry LeafEntry;
struct LeafEntry
{
    Rectangle mbr; // Minimum bounding rectangle
    int obj_id;    // Pointer to the object description record
};
typedef struct LeafNode LeafNode;
struct LeafNode
{
    int num_entries;
    struct LeafEntry entries[MAX_CHILDREN];
};

typedef struct NonLeafEntry NonLeafEntry;
struct NonLeafEntry
{
    Rectangle mbr;             // Minimum bounding rectangle
    struct Node *child_ptr;    // Pointer to the child node
    int largest_hilbert_value; // Largest hilbert value among the data records enclosed by the MBR
};

struct NonLeafNode
{
    int num_entries;
    struct NonLeafEntry entries[MAX_CHILDREN];
};
typedef struct Node *NODE;
struct Node
{
    int is_leaf;
    NODE parent_ptr;
    struct NonLeafNode non_leaf_node; // Non-leaf node
    struct LeafNode leaf_node;        // Leaf node
};

typedef struct HilbertRTree HilbertRTree;
struct HilbertRTree
{
    int height;        // Height of the tree
    struct Node *root; // Root node of the tree
};

// ----------------------------FUNCTION DECLERATIONS------------------------------
HilbertRTree *new_hilbertRTree();
NODE new_node(int is_leaf);
void InsertNode(NODE parent, NODE newNode);

uint32_t interleave(uint32_t x);
uint32_t hilbertXYToIndex(uint32_t n, uint32_t x, uint32_t y);
uint32_t hilbertValue(Rectangle rect);

NODE Insert(NODE root, Rectangle rectangle);
NODE ChooseLeaf(NODE n, Rectangle r, int h);
void AdjustTree(NODE N, NODE newNode, NODE *S, int s_size);
NODE HandleOverFlow(NODE n, Rectangle rectangle);
void preOrderTraverse(NODE n);

void printMBR(Rectangle r);
void copy_rectangle(Rectangle r1, Rectangle r2);
void adjustLHV(NODE parentNode);
void adjustMBR(NODE parentNode);
void searchGetResults(NODE root, Rectangle rectangle, LeafEntry *results);
bool isInArray(NODE *arr, int size, NODE node);
bool rectangles_equal(Rectangle *rect1, Rectangle *rect2);
bool nodes_equal(NODE node1, NODE node2);
bool intersects(Rectangle r1, Rectangle r2);
int area(Rectangle r);
int calculateLHV(NonLeafEntry entry);
int compare(const void *a, const void *b);
int find_entry_index(NODE n, Rectangle rectangle);
int numberOfSiblings(NODE *S);
Rectangle calculateMBR(Rectangle r1, Rectangle r2);
Rectangle calculateEntryMBR(NonLeafEntry entry);
NODE findLeaf(NODE root, Rectangle rectangle);
NODE *cooperatingSiblings(NODE n);
LeafEntry *search(NODE root, Rectangle rectangle);

// FUNCTIONS TO CREATE NODE AND TREE
HilbertRTree *new_hilbertRTree()
{
    HilbertRTree *tree = (HilbertRTree *)malloc(sizeof(HilbertRTree));
    tree->root = NULL;
    tree->height = 0;
    return tree;
}

NODE new_node(int is_leaf)
{
    NODE node = (NODE)malloc(sizeof(struct Node));
    node->is_leaf = is_leaf;
    node->parent_ptr = NULL;
    node->leaf_node.num_entries = 0;
    node->non_leaf_node.num_entries = 0;
    return node;
}

// IS_LEAF = 1 IF LEAF NODE. M = MAXIMUM NUMBER OF CHILDREN THAT NODE CAN HAVE
NODE root1 = NULL;
NODE root2 = NULL;
// Function to calculate the minimum bounding rectangle that contains two given rectangles

uint32_t interleave(uint32_t x)
{
    x = (x | (x << 8)) & 0x00FF00FF;
    x = (x | (x << 4)) & 0x0F0F0F0F;
    x = (x | (x << 2)) & 0x33333333;
    x = (x | (x << 1)) & 0x55555555;
    return x;
}

int computeLeafLHV(NODE a){
    int LHV = 0;
    if(a == NULL){
        return LHV;
    }
    for(int i = 0; i<a->leaf_node.num_entries; i++){
        if(a->leaf_node.entries[i].mbr.h > LHV){
            LHV = a->leaf_node.entries[i].mbr.h;
        }
    }
    return LHV;
}
int compareNonLeafEntry(const void *a, const void *b)
{
    const struct NonLeafEntry *s1 = a;
    const struct NonLeafEntry *s2 = b;
    return s1->largest_hilbert_value - s2->largest_hilbert_value;
}

//compare: Rectangle
int compare(const void *a, const void *b)
{
    const struct Rectangle *s1 = a;
    const struct Rectangle *s2 = b;
    return s1->h - s2->h;
}

//compare: NODE: struct node* NODE = STRUCT Node*
//A = struct node*

void preOrderTraverse(NODE n)
{
    if (!n)
    {
        return;
    }
    if (n->is_leaf == 1)
    {
        printf("Leaf node\n");
        for (int i = 0; i < n->leaf_node.num_entries; i++)
        {
            printf("Object_ID = %d: ", n->leaf_node.entries[i].obj_id);
            printMBR(n->leaf_node.entries[i].mbr);
        }
    }
    else
    {
        printf("Internal node\n");
        for (int i = 0; i < n->non_leaf_node.num_entries; i++)
        {
            printMBR(n->non_leaf_node.entries[i].mbr);
            preOrderTraverse(n->non_leaf_node.entries[i].child_ptr);
        }
    }
}
uint32_t hilbertXYToIndex(uint32_t n, uint32_t x, uint32_t y)
{
    x = x << (16 - n);
    y = y << (16 - n);

    uint32_t A, B, C, D;

    // Initial prefix scan round, prime with x and y
    {
        uint32_t a = x ^ y;
        uint32_t b = 0xFFFF ^ a;
        uint32_t c = 0xFFFF ^ (x | y);
        uint32_t d = x & (y ^ 0xFFFF);

        A = a | (b >> 1);
        B = (a >> 1) ^ a;

        C = ((c >> 1) ^ (b & (d >> 1))) ^ c;
        D = ((a & (c >> 1)) ^ (d >> 1)) ^ d;
    }

    {
        uint32_t a = A;
        uint32_t b = B;
        uint32_t c = C;
        uint32_t d = D;

        A = ((a & (a >> 2)) ^ (b & (b >> 2)));
        B = ((a & (b >> 2)) ^ (b & ((a ^ b) >> 2)));

        C ^= ((a & (c >> 2)) ^ (b & (d >> 2)));
        D ^= ((b & (c >> 2)) ^ ((a ^ b) & (d >> 2)));
    }

    {
        uint32_t a = A;
        uint32_t b = B;
        uint32_t c = C;
        uint32_t d = D;

        A = ((a & (a >> 4)) ^ (b & (b >> 4)));
        B = ((a & (b >> 4)) ^ (b & ((a ^ b) >> 4)));

        C ^= ((a & (c >> 4)) ^ (b & (d >> 4)));
        D ^= ((b & (c >> 4)) ^ ((a ^ b) & (d >> 4)));
    }

    // Final round and projection
    {
        uint32_t a = A;
        uint32_t b = B;
        uint32_t c = C;
        uint32_t d = D;

        C ^= ((a & (c >> 8)) ^ (b & (d >> 8)));
        D ^= ((b & (c >> 8)) ^ ((a ^ b) & (d >> 8)));
    }

    // Undo transformation prefix scan
    uint32_t a = C ^ (C >> 1);
    uint32_t b = D ^ (D >> 1);

    // Recover index bits
    uint32_t i0 = x ^ y;
    uint32_t i1 = b | (0xFFFF ^ (i0 | a));

    return ((interleave(i1) << 1) | interleave(i0)) >> (32 - 2 * n);
}
bool allNodesFull(NODE *S, int numSiblings)
{
    bool allFull = true;
    for (int i = 0; i < numSiblings; i++)
    {
        if (S[i]->is_leaf == 1)
        {
            if (S[i]->leaf_node.num_entries < M)
            {
                allFull = false;
            }
        }
        else if (S[i]->is_leaf == 0)
        {
            if (S[i]->non_leaf_node.num_entries < M)
            {
                allFull = false;
            }
        }
    }
    return allFull;
}
Rectangle calculateMBR(Rectangle r1, Rectangle r2)
{
    Point bottom_leftNew;
    Point top_rightNew;
    if (r1.bottom_left.x <= r2.bottom_left.x)
    {
        bottom_leftNew.x = r1.bottom_left.x;
    }
    else
    {
        bottom_leftNew.x = r2.bottom_left.x;
    }
    if (r1.bottom_left.y <= r2.bottom_left.y)
    {
        bottom_leftNew.y = r1.bottom_left.y;
    }
    else
    {
        bottom_leftNew.y = r2.bottom_left.y;
    }

    if (r1.top_right.x <= r2.top_right.x)
    {
        top_rightNew.x = r2.top_right.x;
    }
    else
    {
        top_rightNew.x = r1.top_right.x;
    }
    if (r1.top_right.y <= r2.top_right.y)
    {
        top_rightNew.y = r2.top_right.y;
    }
    else
    {
        top_rightNew.y = r1.top_right.y;
    }

    Rectangle new_rect;
    new_rect.bottom_left = bottom_leftNew;
    new_rect.top_right = top_rightNew;
    return new_rect;
}

Rectangle calculateEntryMBR(NonLeafEntry entry)
{
    // FOR EACH NON LEAF ENTRY; CALCULATE MBR FROM CHILD  NODES
    // FIND -> LOWEST X, LOWEST Y  AND HIGHEST X, HIGHEST Y
    Rectangle mbr;
    NODE next_node = entry.child_ptr;
    int low_x = 0;
    int low_y = 0;
    int high_x = 0;
    int high_y = 0;

    // IF LEAF POINTER; FIND THE COORDINATES FROM ENTRIES
    if (next_node != NULL && next_node->is_leaf == 1)
    {
        high_x = next_node->leaf_node.entries[0].mbr.top_right.x;
        high_y = next_node->leaf_node.entries[0].mbr.top_right.y;
        low_x = next_node->leaf_node.entries[0].mbr.top_right.x;
        low_y = next_node->leaf_node.entries[0].mbr.top_right.y;
        for (int i = 0; i < next_node->leaf_node.num_entries; i++)
        {
            Rectangle obj_mbr = next_node->leaf_node.entries[i].mbr;
            low_x = (obj_mbr.bottom_left.x < low_x) ? obj_mbr.bottom_left.x : low_x;
            low_y = (obj_mbr.bottom_left.y < low_y) ? obj_mbr.bottom_left.y : low_y;
            high_x = (obj_mbr.top_right.x > high_x) ? obj_mbr.top_right.x : high_x;
            high_y = (obj_mbr.top_right.y > high_y) ? obj_mbr.top_right.y : high_y;
        }
    }
    else if (next_node->is_leaf == 0)
    {
        // NON LEAF NODE:
        high_x = next_node->non_leaf_node.entries[0].mbr.top_right.x;
        high_y = next_node->non_leaf_node.entries[0].mbr.top_right.y;
        for (int i = 0; i < next_node->non_leaf_node.num_entries; i++)
        {
            Rectangle child_mbr = next_node->non_leaf_node.entries[i].mbr;
            low_x = (child_mbr.bottom_left.x < low_x) ? child_mbr.bottom_left.x : low_x;
            low_y = (child_mbr.bottom_left.y < low_y) ? child_mbr.bottom_left.y : low_y;
            high_x = (child_mbr.top_right.x > high_x) ? child_mbr.top_right.x : high_x;
            high_y = (child_mbr.top_right.y > high_y) ? child_mbr.top_right.y : high_y;
        }
    }
    mbr.bottom_left.x = low_x;
    mbr.bottom_left.y = low_y;
    mbr.top_right.x = high_x;
    mbr.top_right.y = high_y;
    mbr.h = hilbertXYToIndex(5, (mbr.bottom_left.x + mbr.top_right.x) / 2, (mbr.bottom_left.y + mbr.top_right.y) / 2);
    // mbr.h = HilbertValue(mbr.bottom_left, mbr.top_right);
    return mbr;
}

// PRINT THE MBR - TOP RIGHT POINT AND BOTTOM LEFT POINT
void printMBR(Rectangle rect)
{

    printf("MBR = %d %d %d %d\n", rect.bottom_left.x, rect.bottom_left.y, rect.top_right.x, rect.top_right.y);
    return;
}

NonLeafEntry new_nonleafentry(NODE newNode)
{
    NonLeafEntry nle;
    nle.child_ptr = newNode;
    nle.mbr = calculateEntryMBR(nle);
    nle.largest_hilbert_value = calculateLHV(nle);
    return nle;
}

void store_all_entries(NonLeafEntry *E, NODE *S, int *num_entries, int numSiblings)
{
    // FOR EACH SIBLING NODE
    for (int i = 0; i < numSiblings; i++)
    {
        // SIBLING NODE IS NON-LEAF
        if (S[i]->is_leaf == 0)
        {
            // STORE ALL THE NON LEAF ENTRIES OF THE NODE
            for (int j = 0; j < S[i]->non_leaf_node.num_entries; j++)
            {
                E[(*num_entries)++] = S[i]->non_leaf_node.entries[j];
            }
        }
    }
}

NODE HandleOverFlowNode(NODE parentNode, NODE new_node)
{
    printf("HANDLE OVERFLOW NODE CALLED");
    // TO INSERT NEW_NODE AS A CHILD POINTER IN A NON LEAF ENTRY
    NonLeafEntry entry = new_nonleafentry(new_node);

    // SET OF COOPERATING SIBLINGS FOR THE PARENTNODE
    NODE *S = cooperatingSiblings(parentNode);
    int numSiblings = numberOfSiblings(S);

    // SET OF ALL NON LEAF ENTRIES IN PARENTNODE AND SIBLINGS
    int *num_entries = (int *)malloc(sizeof(int));
    *num_entries = 0;
    NonLeafEntry *E = (NonLeafEntry *)malloc(sizeof(struct NonLeafEntry) * MAX_POINTS);
    store_all_entries(E, S, num_entries, numSiblings);
    qsort(E, *num_entries, sizeof(NonLeafEntry), compareNonLeafEntry);

    // IF ALL SIBLINGS ARE FULL OR NOT
    bool allFull = true;
    allFull = allNodesFull(S, numSiblings); // True if all nodes in S are full

    // IF ALL SIBLINGS ARE FULL
    if (allFull)
    {
        // ENTRIES PER NODE AND REMAINDER ENTRIES
        int num_entries_per_node = (*num_entries) / numSiblings;
        int remainder_entries = (*num_entries) % numSiblings;

        int *distributionList = (int *)calloc(numSiblings, sizeof(int));
        for (int i = 0; i < numSiblings; i++)
        {
            distributionList[i] = num_entries_per_node;
        }
        for (int j = 0; j < remainder_entries; j++)
        {
            distributionList[j]++;
        }

        int done = 0;
        for (int j = 0; j < numSiblings; j++)
        {
            S[j]->non_leaf_node.num_entries = 0;
            for (int l = 0; l < distributionList[j]; l++)
            {

                S[j]->non_leaf_node.entries[l] = E[done];
                S[j]->non_leaf_node.num_entries++;
                done++;
                
            }
            qsort(E, *num_entries, sizeof(struct NonLeafEntry *), compareNonLeafEntry);

            for (int l = distributionList[j]; l < M; l++)
            {
                S[j]->non_leaf_node.entries[l].mbr.bottom_left.x = 0;
                S[j]->non_leaf_node.entries[l].mbr.bottom_left.y = 0;
                S[j]->non_leaf_node.entries[l].mbr.top_right.x = 0;
                S[j]->non_leaf_node.entries[l].mbr.top_right.y = 0;
                S[j]->non_leaf_node.entries[l].mbr.h = 0;
            }
        }

        return NULL;
        // DISTRIBUTE EVENLY
    }
    else
    {

        qsort(E, *num_entries, sizeof(struct NonLeafEntry *), compareNonLeafEntry);

        NODE NN = (NODE)calloc(1, sizeof(struct Node));
        NN->parent_ptr = NULL;

        if (parentNode->parent_ptr == NULL)
        {
            root_split = true;
        }
        // ADD NN TO SIBLINGS
        S[numSiblings] = NN; // ADD THE NEW NODE TO THE SET OF SIBLINGS
        numSiblings++;

        // qsort(E, n, sizeof(struct Rectangle), compare);
        int num_entries_per_node = (*num_entries) / numSiblings;
        int remainder_entries = (*num_entries) % numSiblings;

        // FOR EACH SIBLING NODE
        int *distributionList = (int *)calloc(numSiblings, sizeof(int));
        for (int i = 0; i < numSiblings; i++)
        {
            distributionList[i] = num_entries_per_node;
        }
        for (int j = 0; j < remainder_entries; j++)
        {
            distributionList[j]++;
        }

        int done = 0;
        for (int j = 0; j < numSiblings; j++)
        {
            S[j]->non_leaf_node.num_entries = 0;
            for (int l = 0; l < distributionList[j]; l++)
            {

                S[j]->non_leaf_node.entries[l] = E[done];
                S[j]->non_leaf_node.num_entries++;
                done++;
            }
            for (int l = distributionList[j]; l < M; l++)
            {
                S[j]->non_leaf_node.entries[l].mbr.bottom_left.x = 0;
                S[j]->non_leaf_node.entries[l].mbr.bottom_left.y = 0;
                S[j]->non_leaf_node.entries[l].mbr.top_right.x = 0;
                S[j]->non_leaf_node.entries[l].mbr.top_right.y = 0;
                S[j]->non_leaf_node.entries[l].mbr.h = 0;
            }
        }
        return NN;
    }
    return NULL;
}

// Compute the Hilbert value for a given rectangle defined by its bottom-left and top-right corners
unsigned long long HilbertValue(Point bottom_left, Point top_right)
{
    // Determine the length of the longest edge of the rectangle
    double max_side = fmax(top_right.x - bottom_left.x, top_right.y - bottom_left.y);

    // Compute the number of levels in the Hilbert curve
    int num_levels = ceil(log2(max_side));

    // Compute the number of cells in the Hilbert curve
    unsigned long long num_cells = pow(2, 2 * num_levels);

    // Compute the Hilbert value for the given rectangle
    unsigned long long hilbert_value = 0;
    for (int i = num_levels - 1; i >= 0; i--)
    {
        unsigned long long mask = 1 << i;
        double x = (bottom_left.x & mask) >> i;
        double y = (bottom_left.y & mask) >> i;
        hilbert_value += mask * pow((3 * x), y);

        if (y == 0)
        {
            if (x == 1)
            {
                bottom_left.x = max_side - bottom_left.x;
                top_right.x = max_side - top_right.x;
            }
            double tmp = bottom_left.x;
            bottom_left.x = bottom_left.y;
            bottom_left.y = tmp;
            tmp = top_right.x;
            top_right.x = top_right.y;
            top_right.y = tmp;
        }
    }

    return hilbert_value;
}
bool rectangles_equal(Rectangle *rect1, Rectangle *rect2)
{
    if (rect1->bottom_left.x != rect2->bottom_left.x ||
        rect1->bottom_left.y != rect2->bottom_left.y ||
        rect1->top_right.x != rect2->top_right.x ||
        rect1->top_right.y != rect2->top_right.y)
    {
        return false;
    }
    return true;
}


bool nodes_equal(NODE node1, NODE node2)
{
    if (node1->is_leaf != node2->is_leaf)
    {
        return false;
    }
    if (node1->is_leaf)
    {
        // Compare LeafNode fields
        struct LeafNode *leaf1 = &node1->leaf_node;
        struct LeafNode *leaf2 = &node2->leaf_node;
        if (leaf1->num_entries != leaf2->num_entries)
        {
            return false;
        }
        for (int i = 0; i < leaf1->num_entries; i++)
        {
            if (rectangles_equal(&leaf1->entries[i].mbr, &leaf2->entries[i].mbr) &&
                leaf1->entries[i].obj_id == leaf2->entries[i].obj_id)
            {
                continue;
            }
            else
            {
                return false;
            }
        }
    }
    else
    {
        // Compare NonLeafNode fields
        struct NonLeafNode *non_leaf1 = &node1->non_leaf_node;
        struct NonLeafNode *non_leaf2 = &node2->non_leaf_node;
        if (non_leaf1->num_entries != non_leaf2->num_entries)
        {
            return false;
        }
        for (int i = 0; i < non_leaf1->num_entries; i++)
        {
            if (rectangles_equal(&non_leaf1->entries[i].mbr, &non_leaf2->entries[i].mbr) &&
                non_leaf1->entries[i].child_ptr == non_leaf2->entries[i].child_ptr &&
                non_leaf1->entries[i].largest_hilbert_value == non_leaf2->entries[i].largest_hilbert_value)
            {
                continue;
            }
            else
            {
                return false;
            }
        }
    }
    return true;
}

// CALCULATE LHV FOR A NON LEAF ENTRY: BY EVAUATING ALL CHILD PTR ETRIES
int calculateLHV(NonLeafEntry entry)
{
    int max_h = 0;
    NODE node = entry.child_ptr;
    // CALCULATE MAXIMUM H OF NODE
    if (node->is_leaf == 1)
    {
        max_h = node->leaf_node.entries[0].mbr.h;
        for (int i = 0; i < node->leaf_node.num_entries; i++)
        {
            if (node->leaf_node.entries[i].mbr.h > max_h)
            {
                max_h = node->leaf_node.entries[i].mbr.h;
            }
        }
        return max_h;
    }
    else if (node->is_leaf == 0)
    {
        max_h = node->non_leaf_node.entries[0].mbr.h;
        // NON LEAF CHILD NODE
        for (int i = 0; i < node->non_leaf_node.num_entries; i++)
        {
            if (node->non_leaf_node.entries[i].mbr.h > max_h)
            {
                max_h = node->non_leaf_node.entries[i].mbr.h;
            }
        }
        return max_h;
    }
    return max_h;
}
// HELPER FUNCTION TO CHECK IN ARRAY
bool isInArray(NODE *arr, int size, NODE node)
{
    for (int i = 0; i < size; i++)
    {
        if (nodes_equal(arr[i], node))
        {
            return true;
        }
    }
    return false;
}

// Assumes the maximum x and y coordinates are 2^31-1
#define MAX_COORDINATE 2147483647

/*ADJUST TREE ASCEND FROM LEAF TOWARDS ROOT AND ADJUST MBR AND LHV VALUES*/
void adjustLHV(NODE parentNode)
{
    if (parentNode != NULL)
    {
        for (int i = 0; i < parentNode->non_leaf_node.num_entries; i++)
        {
            parentNode->non_leaf_node.entries[i].largest_hilbert_value = calculateLHV(parentNode->non_leaf_node.entries[i]);
        }
        adjustLHV(parentNode->parent_ptr);
    }
}
void adjustMBR(NODE parentNode)
{
    if (parentNode == NULL)
    {
        return;
    }
    for (int i = 0; i < parentNode->non_leaf_node.num_entries; i++)
    {
        parentNode->non_leaf_node.entries[i].mbr = calculateEntryMBR(parentNode->non_leaf_node.entries[i]);
    }
    adjustMBR(parentNode->parent_ptr);
}
void InsertNode(NODE parent, NODE newNode)
{
    newNode->parent_ptr = parent;

    // NON LEAF ENTRY CREATED
    NonLeafEntry entry;
    entry.child_ptr = newNode;
    entry.largest_hilbert_value = calculateLHV(entry);
    entry.mbr = calculateEntryMBR(entry);

    // INSERT THE NEW NON LEAF ENTRY ACCORDING TO LHV
    int i = 0;
    while (i < parent->non_leaf_node.num_entries && parent->non_leaf_node.entries[i].largest_hilbert_value <= entry.largest_hilbert_value)
    {
        i++;
    }

    // SHIFT VALUES IN PARENT NODE
    for (int j = parent->non_leaf_node.num_entries; j > i; j--)
    {
        parent->non_leaf_node.entries[j] = parent->non_leaf_node.entries[j - 1];
    }

    // INSERT ACCORDING TO HILBERT VALUE
    parent->non_leaf_node.entries[i] = entry;
    parent->non_leaf_node.num_entries++;

    for(int j = 0; j<parent->non_leaf_node.num_entries; j++){
        printf("LHV = %d\n", parent->non_leaf_node.entries[j].largest_hilbert_value);
    }
}

NODE ChooseLeaf(NODE n, Rectangle r, int h)
{
    /* RETURNS THE LEAF NODE IN WHICH TO PLACE A NEW RECTANGE*/
    /* IF N IS A LEAF, RETURN N*/
    if (n->is_leaf == 1)
    {
        return n;
    }
    /* IF N IS A NON-LEAF NODE, CHOOSE ENTRY (R, PTR, LHV) WITH MINIMUM LHV GREATER THAN H*/
    float min_LHV = n->non_leaf_node.entries[0].largest_hilbert_value;
    NODE next_node = NULL;
    for (int i = 0; i < n->non_leaf_node.num_entries; i++)
    {
        if (n->non_leaf_node.entries[i].largest_hilbert_value >= h && n->non_leaf_node.entries[i].largest_hilbert_value <= min_LHV)
        {
            min_LHV = n->non_leaf_node.entries[i].largest_hilbert_value;
            next_node = n->non_leaf_node.entries[i].child_ptr;
        }
    }
    /* IF ALL CHILDREN HAVE LHV LESS THAN H */
    if (next_node == NULL)
    {
        min_LHV = n->non_leaf_node.entries[0].largest_hilbert_value;
        // CHOOSE THE CHILD NODE WITH LARGEST LHV
        for (int i = 0; i < n->non_leaf_node.num_entries; i++)
        {
            if (n->non_leaf_node.entries[i].largest_hilbert_value >= min_LHV)
            {
                min_LHV = n->non_leaf_node.entries[i].largest_hilbert_value;
                next_node = n->non_leaf_node.entries[i].child_ptr;
            }
        }
    }

    /* DESCEND UNTIL A LEAF NODE IS REACHED*/
    return ChooseLeaf(next_node, r, h);
}
//SORT THE SIBLINGS USING BUBBLE SORT
void sortSiblings(NODE *S, int numSiblings)
{
    // bubble sort implementation
    for (int i = 0; i < numSiblings - 1; i++)
    {
        for (int j = 0; j < numSiblings - i - 1; j++)
        {
            if (computeLeafLHV(S[j]) > computeLeafLHV(S[j + 1]))
            {
                NODE temp = S[j];
                S[j] = S[j + 1];
                S[j + 1] = temp;
            }
        }
    }
}
NODE *cooperatingSiblings(NODE n)
{
    // taking s=2
    NODE *S = (NODE *)malloc(sizeof(NODE) * MAX_POINTS);
    for (int i = 0; i < MAX_POINTS; i++)
    {
        S[i] = NULL;
    }
    S[0] = n;
    int numSiblingsCP = 0;
    NODE parentNode = n->parent_ptr;
    if (parentNode == NULL)
    {
        return S;
    }
    // GO FROM CURRENT NODE TO ROOT FINDING SIBLINGS
    // WITH LESS THAN MAXIMUM POINTERS
    int index = -1;
    for (int i = 0; i < parentNode->non_leaf_node.num_entries; i++)
    {
        if (parentNode->non_leaf_node.entries[i].child_ptr == n)
        {
            index = i;
            break;
        }
    }
    if (index > 0)
    {
        S[++numSiblingsCP] = parentNode->non_leaf_node.entries[index - 1].child_ptr;
    }
    if (index < parentNode->non_leaf_node.num_entries - 1)
    {
        S[++numSiblingsCP] = parentNode->non_leaf_node.entries[index + 1].child_ptr;
    }

    //SORT S ON COMPUTELHV(ELEMENT)
    sortSiblings(S, numSiblingsCP + 1);
    return S; 
}

int numberOfSiblings(NODE *S)
{
    int numSiblings = 0;
    for (int i = 0; i < MAX_POINTS; i++)
    {
        if (S[i] != NULL)
        {
            numSiblings++;
        }
    }
    return numSiblings;
}
void copy_rectangle(Rectangle r1, Rectangle r2)
{
    r1.bottom_left.x = r2.bottom_left.x;
    r1.top_right.x = r2.top_right.x;
    r1.bottom_left.y = r2.bottom_left.y;
    r1.top_right.y = r2.top_right.y;
}

NODE HandleOverFlow(NODE n, Rectangle rectangle)
{
    // E = SET OF ALL ENTRIES FROM N AND S-1 COOPERATING SIBLINGS
    NODE *S = cooperatingSiblings(n);      // CONTAINS COOPERATING SIBLINGS AND NODE
    int numSiblings = numberOfSiblings(S); // SIZE OF SET S
    int num_entries = 0;                   // SIZE OF SET E
    Rectangle* E = (Rectangle*)calloc(MAX_POINTS, sizeof(struct Rectangle));

    // H1: SET OF ALL ENTRIES FROM SIBLINGS
    for (int i = 0; i < numSiblings; i++)
    {
        // SIBLING NODE IS A LEAF
        if (S[i]->is_leaf == 1)
        {
            // STORES THE MBR OF ENTRIES INTO THE E ARRAY
            for (int j = 0; j <S[i]->leaf_node.num_entries; j++)
            {
                E[num_entries++] = S[i]->leaf_node.entries[j].mbr;
            }
        }
        // SIBLING NODE IS NON-LEAF
        else
        {
            // STORES THE MBR OF ENTRIES INTO THE E ARRAY
            for (int j = 0; j <S[i]->non_leaf_node.num_entries; j++)
            {
                E[num_entries++] = S[i]->non_leaf_node.entries[j].mbr;
            }
        }
    }

    // ADD R TO E
    E[num_entries++] = rectangle;

    // CHECK IF ANY NODE IS NOT FULL
    bool allFull = true;
    for (int i = 0; i < numSiblings; i++)
    {
        if (S[i]->is_leaf == 1)
        {
            if (S[i]->leaf_node.num_entries < M)
            {
                allFull = false;
                break;
            }
        }
        else if (S[i]->is_leaf == 0)
        {
            if (S[i]->non_leaf_node.num_entries < M)
            {
                allFull = false;
                break;
            }
        }
    }

    // IF ATLEAST ONE OF SIBLINGS IS NOT FULL
    if (!allFull)
    {
        /*num_entries: total number of entries*/
        /*num_siblings+1: total number of siblings to distribute in */

        // SORT THE ENTRIES IN E BASED ON THEIR HILBERT VALUE
        qsort(E, num_entries, sizeof(Rectangle), compare);
        for(int i = 0; i<num_entries; i++){
            printf("STORED: %d\n", E[i].h);
        }
        printf("NOT ALL FULL: %d\n", rectangle.h);
        printf("ENTRIES: %d SIBLINGS: %d\n", num_entries, numSiblings);
        // ENTRIES PER NODE AND REMAINDER ENTRIES
        int num_entries_per_node = num_entries / numSiblings;
        int remainder_entries = num_entries % numSiblings;

        int *distributionList = (int *)calloc(numSiblings, sizeof(int));
        for (int i = 0; i < numSiblings; i++)
        {
            distributionList[i] = num_entries_per_node;
        }
        for (int j = 0; j < remainder_entries; j++)
        {
            distributionList[j]++;
        }

        int done = 0;
        for (int j = 0; j < numSiblings; j++)
        {
            S[j]->leaf_node.num_entries = 0;
            for (int l = 0; l < distributionList[j]; l++)
            {

                S[j]->leaf_node.entries[l].mbr = E[done++];
                S[j]->leaf_node.entries[l].obj_id = ++CURRENT_ID;
                S[j]->leaf_node.num_entries++;
        
            }
            for (int l = distributionList[j]; l < M; l++)
            {
                S[j]->leaf_node.entries[l].mbr.bottom_left.x = 0;
                S[j]->leaf_node.entries[l].mbr.bottom_left.y = 0;
                S[j]->leaf_node.entries[l].mbr.top_right.x = 0;
                S[j]->leaf_node.entries[l].mbr.top_right.y = 0;
                S[j]->leaf_node.entries[l].mbr.h = 0;
                S[j]->leaf_node.entries[l].obj_id = 0;
            }
        }

        return NULL;
        // DISTRIBUTE E EVENLY AMONG THE S NODES ACCORDING TO THE HILBERT VALUE
    }
    // IF ALL COOPERATING SIBLINGS ARE FULL
    else
    {
        // DISTRIBUTE E EVENLY AMONG THE S+1 NODES ACCORDING TO THE HILBERT VALUE
        // CREATE A NEW NODE NN
        NODE NN = (NODE)malloc(sizeof(struct Node));
        NN->parent_ptr = NULL;
        NN->leaf_node.num_entries = 0;
        NN->non_leaf_node.num_entries = 0;
        NN->is_leaf = 1;

        // IF ROOT WAS SPLIT TO CREATE NEW NODE
        if (n->parent_ptr == NULL)
        {
            root_split = true;
        }
        // ADD NN TO SIBLINGS
        S[numSiblings] = NN; // ADD THE NEW NODE TO THE SET OF SIBLINGS
        numSiblings++;

        int n1 = sizeof(E) / sizeof(E[0]);
        qsort(E, n1, sizeof(struct Rectangle), compare);


        int num_entries_per_node = num_entries / numSiblings;
        int remainder_entries = num_entries % numSiblings;

        // FOR EACH SIBLING NODE
        int *distributionList = (int *)calloc(numSiblings, sizeof(int));
        for (int i = 0; i < numSiblings; i++)
        {
            distributionList[i] = num_entries_per_node;
        }
        for (int j = 0; j < remainder_entries; j++)
        {
            distributionList[j]++;
        }

        int done = 0;
        for (int j = 0; j < numSiblings; j++)
        {
            S[j]->leaf_node.num_entries = 0;
            for (int l = 0; l < distributionList[j]; l++)
            {

                S[j]->leaf_node.entries[l].mbr = E[done];
                S[j]->leaf_node.entries[l].obj_id = ++CURRENT_ID;
                S[j]->leaf_node.num_entries++;
                done++;
            }
            for (int l = distributionList[j]; l < M; l++)
            {
                S[j]->leaf_node.entries[l].mbr.bottom_left.x = 0;
                S[j]->leaf_node.entries[l].mbr.bottom_left.y = 0;
                S[j]->leaf_node.entries[l].mbr.top_right.x = 0;
                S[j]->leaf_node.entries[l].mbr.top_right.y = 0;
                S[j]->leaf_node.entries[l].mbr.h = 0;
                S[j]->leaf_node.entries[l].obj_id = 0;
            }
        }
        return NN;
    }
    return NULL;
}
// RETURNS TRUE IF ROOT NODE WAS SPLIT
void AdjustTree(NODE N, NODE NN, NODE *S, int s_size)
{
    // STOP IF ROOT LEVEL REACHED
    NODE Np = N->parent_ptr;
    NODE new_node = NULL;

    // PARENT = NULL; ROOT LEVEL
    if (!Np)
    {
        return;
    }
    // INSERT SPLIT NODE INTO PARENT
    if (NN)
    {
        // INSERT IN CORRECT ORDER IF ROOM IN PARENT NODE
        if (Np->non_leaf_node.num_entries < MAX_CHILDREN)
        {
            InsertNode(Np, NN);
        }
        else
        {
            // PARENT NODE MUST BE SPLIT

            //->>TO BE IMPLEMENTED: HANDLEOVERFLOWNODE: WHEN PARENT NODE MUST BE SPLIT
            new_node = HandleOverFlowNode(Np, NN);
            // IF ROOT NODE WAS SPLIT BH HANDLEOVERFLOW
            if (Np->parent_ptr == NULL && new_node != NULL)
            {
                root_split = true;
                root1 = Np;
                root2 = new_node;
            }
        }
    }
    // ADJUST MBR AND LHV IN PARENT LEVEL
    // P = SET OF PARENT NODES FOR NODES IN S
    NODE *P = (NODE *)malloc(sizeof(struct Node) * MAX_POINTS);
    int numParents = 0;
    for (int i = 0; i < s_size; i++)
    {
        if (S[i]->parent_ptr == NULL)
        {
            continue;
        }

        NODE parent = S[i]->parent_ptr;
        if (parent && !isInArray(P, numParents, parent))
        {
            P[numParents++] = parent;
        }
    }
    // ADJUST MBR AND LHV VALUES
    for (int i = 0; i < numParents; i++)
    {
        NODE parent = P[i];
        adjustMBR(parent);
        adjustLHV(parent);
    }

    // NEXT LEVEL
    AdjustTree(Np, new_node, P, numParents);
}

NODE Insert(NODE root, Rectangle rectangle)
{

    NODE leafNode = ChooseLeaf(root, rectangle, rectangle.h);
    NODE newLeafNode = NULL;

    if (leafNode->leaf_node.num_entries < M)
    {
        // LEAF NODE HAS SPACE
        printf("LEAF NODE HAS SPACE: %d\n", rectangle.h);
        int i = 0;
        while (i < leafNode->leaf_node.num_entries && leafNode->leaf_node.entries[i].mbr.h < rectangle.h)
        {
            i++;
        }

        for (int j = leafNode->leaf_node.num_entries; j > i; j--)
        {
            leafNode->leaf_node.entries[j] = leafNode->leaf_node.entries[j - 1];
        }
        // INSERT ACCORDING TO HILBERT ORDER AND RETURN
        LeafEntry entry;
        entry.mbr = rectangle;
        entry.obj_id = ++CURRENT_ID;
        leafNode->leaf_node.entries[i] = entry;
        leafNode->leaf_node.num_entries++;

        root_split = false;
        NODE* S = cooperatingSiblings(leafNode);
        int numSiblings = numberOfSiblings(S);
        AdjustTree(leafNode, newLeafNode, S, numSiblings);
    
        return root;
    }


    else
    {
        // LEAF NODE IS FULL
        printf("LEAF NODE IS FULL: %d\n", rectangle.h);
        newLeafNode = HandleOverFlow(leafNode, rectangle);
        NODE* S = cooperatingSiblings(leafNode);
        int numSiblings = numberOfSiblings(S);

        if (newLeafNode)
        {
            newLeafNode->is_leaf = 1;
            S[numSiblings++] = newLeafNode;
        }

        root_split = false;
        AdjustTree(leafNode, newLeafNode, S, numSiblings);

        if (leafNode->parent_ptr == NULL && newLeafNode != NULL)
        {

            NODE newRoot = (NODE)malloc(sizeof(struct Node));
            newRoot->is_leaf = 0;
            newRoot->non_leaf_node.num_entries = 2;
            newRoot->parent_ptr = NULL;

            newRoot->non_leaf_node.entries[0].child_ptr = leafNode;
            newRoot->non_leaf_node.entries[0].largest_hilbert_value = calculateLHV(newRoot->non_leaf_node.entries[0]);
            newRoot->non_leaf_node.entries[0].mbr = calculateEntryMBR(newRoot->non_leaf_node.entries[0]);

            newRoot->non_leaf_node.entries[1].child_ptr = newLeafNode;
            newRoot->non_leaf_node.entries[1].largest_hilbert_value = calculateLHV(newRoot->non_leaf_node.entries[1]);
            newRoot->non_leaf_node.entries[1].mbr = calculateEntryMBR(newRoot->non_leaf_node.entries[1]);

            leafNode->parent_ptr = newRoot;
            newLeafNode->parent_ptr = newRoot;
            return newRoot;
        }
        // RETURNS THE NEW LEAF IF SPLIT WAS INEVITABLE
    }

    // Propogate changes upward
    // FORM A SET S CONTAINING L: COOPERATING SIBLINGS AND NEW LEAF (IF ANY)

    root_split = false;

    // IF NODE SPLIT CAUSED ROOT TO SPLIT, CREATEA NEWROOT WITH CHILDREN
    // AS RESULTING NODES
    //  Check if the root split
    if (root_split)
    {
        NODE newRoot = (NODE)malloc(sizeof(struct Node));
        newRoot->is_leaf = 0;
        newRoot->non_leaf_node.num_entries = 2;
        newRoot->parent_ptr = NULL;

        newRoot->non_leaf_node.entries[0].child_ptr = root1;
        newRoot->non_leaf_node.entries[0].largest_hilbert_value = calculateLHV(newRoot->non_leaf_node.entries[0]);
        newRoot->non_leaf_node.entries[0].mbr = calculateEntryMBR(newRoot->non_leaf_node.entries[0]);

        newRoot->non_leaf_node.entries[1].child_ptr = root2;
        newRoot->non_leaf_node.entries[1].largest_hilbert_value = calculateLHV(newRoot->non_leaf_node.entries[1]);
        newRoot->non_leaf_node.entries[1].mbr = calculateEntryMBR(newRoot->non_leaf_node.entries[1]);

        root1->parent_ptr = newRoot;
        root2->parent_ptr = newRoot;
        return newRoot;
    }
    return root;
}

// SEARCH ALGORITHM
//-> NONLEAF - THOSE WITH MBR INTERSECTING THE QUERY WINDOW W
//-> LEAF - THOSE WITH MBR INTERSECTING THE QUERY WINDOW W
/*ALL RECTANGLES THAT OVERLAP A SEARCH RECTANGLE*/
bool intersects(Rectangle r1, Rectangle r2)
{
    return !(
        r1.top_right.x < r2.bottom_left.x || r2.top_right.x < r1.bottom_left.x ||
        r1.top_right.y < r2.bottom_left.y || r2.top_right.y < r1.bottom_left.y);
}
// SEARCH SHOULD RETURN AN ARRAY OF RESULTS
void searchGetResults(NODE root, Rectangle rectangle, LeafEntry *results)
{
    if (root->is_leaf == 1)
    {
        for (int i = 0; i < root->leaf_node.num_entries; i++)
        {
            if (intersects(root->leaf_node.entries[i].mbr, rectangle))
            {
                results[num_results++] = root->leaf_node.entries[i];
            }
        }
    }
    else
    {
        for (int i = 0; i < root->non_leaf_node.num_entries; i++)
        {
            if (intersects(root->non_leaf_node.entries[i].mbr, rectangle))
            {
                searchGetResults(root->non_leaf_node.entries[i].child_ptr, rectangle, results);
            }
        }
    }
}
LeafEntry *search(NODE root, Rectangle rectangle)
{
    // NUMBER OF RESULTS: STORED IN GLOBAL VARIABLE
    num_results = 0;
    LeafEntry *results = (LeafEntry *)malloc(sizeof(LeafEntry) * MAX_POINTS);
    searchGetResults(root, rectangle, results);
    return results;
}
// handle overflow

/*FIND THE LEAF NODE CONTAINING A RECTANGLE R*/
NODE findLeaf(NODE root, Rectangle rectangle)
{
    if (root->is_leaf == 1)
    {
        return root;
    }

    /* IF NON LEAF; FIND OVERLAPPING ENTRY*/
    int i;
    for (i = 0; i < root->non_leaf_node.num_entries; i++)
    {
        if (intersects(root->non_leaf_node.entries[i].mbr, rectangle))
        {
            break;
        }
    }
    return findLeaf(root->non_leaf_node.entries[i].child_ptr, rectangle);
}

int find_entry_index(NODE n, Rectangle rectangle)
{
    int index = -1;
    for (int i = 0; i < n->leaf_node.num_entries; i++)
    {
        if (n->leaf_node.entries[i].mbr.bottom_left.x == rectangle.bottom_left.x &&
            n->leaf_node.entries[i].mbr.bottom_left.y == rectangle.bottom_left.y &&
            n->leaf_node.entries[i].mbr.top_right.x == rectangle.top_right.x &&
            n->leaf_node.entries[i].mbr.top_right.y == rectangle.top_right.y)
        {
            index = i;
            break;
        }
    }
    return index;
}



void print_mbr(Rectangle r)
{
    printf("Top right point: (%d, %d)\n", r.top_right.x, r.top_right.y);
    printf("Bottom left point: (%d, %d)\n", r.bottom_left.x, r.bottom_left.y);
}

void traverse(NODE n)
{
    if (n->is_leaf == 1)
    {
        printf("Leaf node\n");
        for (int i = 0; i < n->leaf_node.num_entries; i++)
        {
            printf("Object_ID = %d: ", n->leaf_node.entries[i].obj_id);
            printMBR(n->leaf_node.entries[i].mbr);
        }
    }
    else
    {
        printf("Internal node\n");
        for (int i = 0; i < n->non_leaf_node.num_entries; i++)
        {
            printMBR(n->non_leaf_node.entries[i].mbr);
            traverse(n->non_leaf_node.entries[i].child_ptr);
        }
    }
}

// Call this function with the root node to traverse the whole tree
void traverse_tree(HilbertRTree *tree)
{
    traverse(tree->root);
}

int main()
{
    FILE *fp;
    Point points[MAX_POINTS];
    int num_points = 0;

    // Open the file containing the data points
    fp = fopen("data.txt", "r");
    if (fp == NULL)
    {
        printf("Error opening file.\n");
        return 1;
    }
    for (int i = 0; i < MAX_POINTS; i++)
    {
        fscanf(fp, "%d %d\n", &points[num_points].x, &points[num_points].y);
        printf("POINT  = %d %d\n", points[num_points].x, points[num_points].y);
        num_points++;
    }
    fclose(fp);

    // LETS ASSUME THE RECTANGLES INITIALLY ARE THE DATA POINTS
    // AND THEIR CENTRES ARE THE POINTS TOO
    //  CREATE RECTANGLES
    Rectangle rectangles[MAX_POINTS];
    for (int i = 0; i < MAX_POINTS; i++)
    {
        rectangles[i].bottom_left = points[i];
        rectangles[i].top_right = points[i];
        rectangles[i].h = hilbertXYToIndex(5, points[i].x, points[i].y);

        // rectangles[i].h = HilbertValue(points[i],points[i]);
    }
    // QUICK SORT RECTANGLES ARRAY
    int number_of_rectangles = sizeof(rectangles) / sizeof(rectangles[0]);
    qsort(rectangles, number_of_rectangles, sizeof(struct Rectangle), compare);

    for(int i = 0; i<MAX_POINTS; i++){
        printf("RECTANGLE WITH H-VALUE = %d\n", rectangles[i].h);
    }

    HilbertRTree *Rtree = new_hilbertRTree();
    Rtree->root = new_node(1);
    Rtree->root->parent_ptr = NULL;

    for (int i = 0; i < MAX_POINTS; i++)
    {
        // if(rectangles[i].h == 310){
        //     continue;
        // }
        Rtree->root = Insert(Rtree->root, rectangles[i]);
        
    }
    return 0;

}
