#include <iostream>

using namespace std;
struct node
{
    node* next=nullptr;
    node* last=nullptr;
    void* data=nullptr;
    
};

node* next(node* curr)
{
    return curr -> next;
}

node* last(node* curr)
{
    return curr -> last;
}

node* add_node(node* curr, void* Input_data)
{
    node* next_node = new node;
    next_node->data=Input_data;
    node* temp = curr->next;
    curr->next=next_node;
    next_node->last= curr;
    next_node->next= temp;
    return next_node;
}

node* remove_node(node* curr)
{
    node* prev = curr->last;
    delete curr;
    return prev;
}