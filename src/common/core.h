
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/**
What happens if a C string is not null-terminated

A C string is just an array of char ending with '\0' (null character).
That null terminator is how all standard C string functions know where the string ends.

If the null terminator is missing, C has no idea where the string ends — and that leads to undefined behavior.
 */
#define MAX_NAME_CHARS 11 // 10 + 1 --> 10 chars is sent from Java + 1 for NULL character
#define MAX_PHONE_CHARS 21 // 20 + 1 --> 20 chars is sent from Java + 1 for NULL character
#define MAX_EMAIL_CHARS 21 //20 + 1
#define INVALID_INT -1  
#define CUSTOMER_INFO_JSON_STR_SIZE 200 
/*
Lets understand why the json string equivalent of struct customer_info will be about 200 bytes long.

Let’s compute approximate memory layout for customer_info on a 64-bit (x86_64) system:

Field	        Type	    Size (bytes)
custId	        int	        4
firstName	    char[10]	10
lastName	    char[10]	10
phone	        char[20]	20
email	        char[20]	20
balance	        double	    8
next	        pointer 	8
Subtotal		80 bytes

You can verify if it is indeed 80 bytes by using sizeof operator on the struct.

Now, a JSON string of the struct might look like:
{
  "id": 1,
  "firstName": "Binita",
  "lastName": "Bharati",
  "phone": "984516786",
  "email": "binita@gmail.com",
  "balance": 100.45
}
If all fields are maximum length (e.g., firstName=10, lastName=10, phone=20, email=20), your JSON might reach ≈180–200 bytes.
*/

typedef struct customer_info {
    int custId;//key

    //values
    char firstName[MAX_NAME_CHARS];
    char lastName[MAX_NAME_CHARS];
    char phone[MAX_PHONE_CHARS];
    char email[MAX_EMAIL_CHARS];
    double balance; 

    //next entry
    struct customer_info* next;
}customer_info;

customer_info* transform(int custId, char firstName[MAX_NAME_CHARS], char lastName[MAX_NAME_CHARS],
char phone[MAX_PHONE_CHARS], char email[MAX_EMAIL_CHARS], double balance) {
    customer_info *mapEntry = malloc(sizeof(customer_info));

    mapEntry->custId = custId;

    /**
     * It copies at most MAX_NAME_CHARS bytes from firstName into mapEntry->firstName.

    If the source string (firstName) has fewer than MAX_NAME_CHARS characters,
    → it pads the rest with '\0' bytes (nulls). ✅

    If the source string has equal to or more than MAX_NAME_CHARS characters,
    → it copies exactly MAX_NAME_CHARS bytes and does not null-terminate.
     */
    strncpy(mapEntry->firstName, firstName, MAX_NAME_CHARS);
    strncpy(mapEntry->lastName, lastName, MAX_NAME_CHARS);
    strncpy(mapEntry->phone, phone, MAX_PHONE_CHARS);
    strncpy(mapEntry->email, email, MAX_EMAIL_CHARS);
    mapEntry->balance = balance;


    /*If not initialized, next can point to garbage pointer value.
    And, you will get C side errors while accesing next, like the code below from lookup method:

     map_entry *headPtr = hashmap_value;
        map_entry *nextPtr = hashmap_value->next;
        while (nextPtr != NULL) {
            if (nextPtr->key == key) {
            }
        }  
    */
    mapEntry->next = NULL;
    return mapEntry;
}



