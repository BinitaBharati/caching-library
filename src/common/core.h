
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
#define MAX_TRXNS_PER_CUSTOMER 10 
#define MAX_CHARS_IN_YYYY_MM_DD 10 
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
#define CUSTOMER_INFO_JSON_STR_SIZE 200 
/**
 * I have this json structure. { "id": 1234, "trxns": [ { date: 1234, credit: 100.0, debit: 0.0, balance: 445.56 } ] } 
 * Here : id is of type int date of type long credit, debit, balance of type double. 
 * Tell me how much bytes will be occupied by the sample json structure if the max entries in trxns array is upto 10.
 * 
 * We are computing the size of the JSON string in bytes, minified (no spaces/newlines), for:

id → int (32-bit, max 10 digits "2147483647")

date → long (64-bit, max 19 digits "9223372036854775807")

credit/debit/balance → double (max 20 chars including decimal/exponent/sign)

trxns array → up to 10 entries

Part	    Characters
{	            1
"date": 	    7
, + "credit":	10
, + "debit":	 9
, + "balance":	11
}	             1
NULL terminator  1

Total fixed = 1+7+10+9+11+1+1 = 40 bytes

Numeric fields per transaction (max digits):

date → 19

credit → 20

debit → 20

balance → 20

Total numeric = 19+20+20+20 = 79 bytes

✅ Per transaction JSON = 40 + 79 = 119 bytes

Transactions array overhead

Array brackets: [ and ] → 2 bytes

Commas between transactions: 9 commas (for 10 entries) → 9 bytes

Array overhead = 11 bytes

Outer JSON + id field

"id": → 5

max int digits → 10

comma after id → 1

outer { and } → 2

Outer + id = 5 + 10 + 1 + 2 = 18 bytes

Total JSON size for 10 transactions
Per transaction: 119 × 10 = 1190
Array overhead: 11
Outer + id: 18
-------------------
Total ≈ 1190 + 11 + 18 = 1219 bytes

Minified JSON for 1 customer with 10 transactions → ≈1219 bytes (~1.2 KB)
 */
#define CUSTOMER_TRXN_INFO_JSON_STR_SIZE 1300//≈1219 max chars 


typedef struct transaction_details{ 
    long trxnDate; //8 bytes
    double credit; //8 bytes
    double debit; //8 bytes
    double balance; //8 bytes
} transaction_details;//total 32 bytes occupied in memory.

typedef struct customer_info {
    int custId;//key

    //values
    char firstName[MAX_NAME_CHARS];
    char lastName[MAX_NAME_CHARS];
    char phone[MAX_PHONE_CHARS];
    char email[MAX_EMAIL_CHARS];
    double balance; 
    struct transaction_details latest_trxns[MAX_TRXNS_PER_CUSTOMER];

    //next entry
    struct customer_info* next;
}customer_info;

void init_customer_transactions(customer_info* customer_entry) {
    for (int i = 0 ; i < MAX_TRXNS_PER_CUSTOMER ; i++) {
        /* We will identify a dummy transaction by the trxn_date, if the date is -1, that 
        specifies a dummy trxn. If we do not initialize the customer's trxn array, then
        it will hold garbage values, making us incapable of identifying a valid entry
        from invalid entry.
        */

        /**
         * The below code throws this compile time error:
         *      customer_entry->latest_trxns[i] = {-1, 0.0, 0.0, 0.0};
         * src/implementation/../common/core.h:150:43: error: expected expression before ‘{’ token 150 | customer_entry->latest_trxns[i] = {-1, 0.0, 0.0, 0.0};
         * 
         * C does not allow assignment using brace-enclosed initializers ({}) except at declaration time.
         * So, below 
         *              transaction_details t = {-1, 0.0, 0.0, 0.0}; // ✅ allowed (during declaration)
         * is fine, but doing it after declaration (like in your loop) is not allowed in C.
         * 
         * Correct C way
            You can fix it by creating a temporary struct and assigning it:
         */
        transaction_details dummy = { -1, 0.0, 0.0, 0.0 };
        customer_entry->latest_trxns[i] = dummy;   
    }
}

customer_info* transformBasicInputToCustomerInfo(int custId, char firstName[MAX_NAME_CHARS], char lastName[MAX_NAME_CHARS],
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

    init_customer_transactions(mapEntry);
    return mapEntry;
}





