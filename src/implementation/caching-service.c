# include <stdio.h>
# include <stdlib.h>
#include <pthread.h>

#include "../common/model/core.h"

#define TABLE_SIZE 100000000
#define LOCK_COUNT 4096


static customer_info *hash_table[TABLE_SIZE];
//bucket level locks, same as Java's ConcurrentHashMap
static pthread_mutex_t bucket_locks[LOCK_COUNT];


void init_locks() {
    for (int i = 0; i < LOCK_COUNT; i++) {
        pthread_mutex_init(&bucket_locks[i], NULL);
    }
}

void init_hash_table() {
    for (int i = 0 ; i < TABLE_SIZE ; i++) {
        hash_table[i] = NULL;
    }
    init_locks();
}

int get_lock_index(int bucket_index) {
    return bucket_index % (LOCK_COUNT - 1);
} 

unsigned int hash(unsigned int key){
    return key % TABLE_SIZE;
}

void update_hash_table(int custId, char firstName[MAX_NAME_CHARS], char lastName[MAX_NAME_CHARS],
char phone[MAX_PHONE_CHARS], char email[MAX_EMAIL_CHARS], double balance) {
    
    if (custId == INVALID_INT) {
        return;
    }

    //customer_info* mapEntry = transform(custId, firstName, lastName, phone, email, balance);

    int index = hash(custId); 
    int lock_index = get_lock_index(index);
    pthread_mutex_lock(&bucket_locks[lock_index]);
    if (hash_table[index] == NULL) {
         customer_info* mapEntry = transform(custId, firstName, lastName, phone, email, balance);
         hash_table[index] = mapEntry;   
         pthread_mutex_unlock(&bucket_locks[lock_index]);
         return;
    }

    customer_info *headPtr = hash_table[index];
    if (headPtr->custId  == custId) {
        //update cache value for the custId.
        strncpy(headPtr->firstName, firstName, MAX_NAME_CHARS);
        strncpy(headPtr->lastName, lastName, MAX_NAME_CHARS);
        strncpy(headPtr->phone, phone, MAX_PHONE_CHARS);
        strncpy(headPtr->email, email, MAX_EMAIL_CHARS);
        headPtr->balance = balance;
        //replace the current node with input node.
        //mapEntry->next = headPtr->next;
        //hash_table[index] = mapEntry;
        //free(headPtr);
        pthread_mutex_unlock(&bucket_locks[lock_index]);
        return;
    }
    else {
        customer_info *prevPtr = headPtr;
        customer_info *nextPtr = headPtr->next;
        while (nextPtr != NULL) {
            if (nextPtr->custId == custId) {
                //replace the current node with input node.
                //mapEntry->next = nextPtr->next;
                //prevPtr->next = mapEntry;  
                //free(nextPtr);
                strncpy(nextPtr->firstName, firstName, MAX_NAME_CHARS);
                strncpy(nextPtr->lastName, lastName, MAX_NAME_CHARS);
                strncpy(nextPtr->phone, phone, MAX_PHONE_CHARS);
                strncpy(nextPtr->email, email, MAX_EMAIL_CHARS);
                nextPtr->balance = balance;
                pthread_mutex_unlock(&bucket_locks[lock_index]);
                return;
        }
        prevPtr = nextPtr;
        nextPtr = nextPtr->next;
    }
    //if control is here that means, we need to add a brand new node at the end of the linked list
    customer_info* mapEntry = transform(custId, firstName, lastName, phone, email, balance);
    mapEntry->next = NULL;
    prevPtr->next = mapEntry;
    pthread_mutex_unlock(&bucket_locks[lock_index]);
    return;
    }   
}

customer_info* hash_table_look_up(unsigned int custId, char* jsonString) {
    customer_info *hashmap_value;
    unsigned int index = hash(custId);
    int lock_index = get_lock_index(index);
    pthread_mutex_lock(&bucket_locks[lock_index]);
    
    hashmap_value = hash_table[index];
    if (hashmap_value != NULL) {
       if (hashmap_value->custId == custId) {
            char* firstName = hashmap_value->firstName;
            char* lastName = hashmap_value->lastName;
            char* phone = hashmap_value->phone;
            char* email = hashmap_value->email;
            double balance = hashmap_value->balance;

            /**
             * It is dangerous to return the original map value as it is to Java, as you need to unlock before returning.
             * Once unlocked, a write(insert_to_hash_table) for the same key may free the memory that is holding this structure. 
             * On Java end, as you try to get the value of the returned char pointer, it may get garbage, as the holder 
             * parent structure itself got collected (free'ed). 
             * 
             * So, you need to return a copy of the original map value to Java, and also inform Java guys to call the
             * corresponding free_string FFI method to free the memory used by this copied string. This is to be done once 
             * Java is done with reading the output from this method.
             * 
             * You wont need seperate free_string FFI call, if you use the char array passed by Java to fill in your final 
             * map value as a string.
             */

            snprintf(jsonString, CUSTOMER_INFO_JSON_STR_SIZE,
            "{"
            "\"id\":%d,"
            "\"firstName\":\"%s\","
            "\"lastName\":\"%s\","
            "\"phone\":\"%s\","
            "\"email\":\"%s\","
            "\"balance\":%.2f"
            "}",
            custId,
            firstName,
            lastName,
            phone,
            email,
            balance
            );
            pthread_mutex_unlock(&bucket_locks[lock_index]);
            return hashmap_value;
       }
       else {
            customer_info *headPtr = hashmap_value;
            customer_info *nextPtr = hashmap_value->next;
            while (nextPtr != NULL) {
                if (nextPtr->custId == custId) {
                    char* firstName = nextPtr->firstName;
                    char* lastName = nextPtr->lastName;
                    char* phone = nextPtr->phone;
                    char* email = nextPtr->email;
                    double balance = nextPtr->balance;

                    snprintf(jsonString, CUSTOMER_INFO_JSON_STR_SIZE,
                    "{"
                    "\"id\":%d,"
                    "\"firstName\":\"%s\","
                    "\"lastName\":\"%s\","
                    "\"phone\":\"%s\","
                    "\"email\":\"%s\","
                    "\"balance\":%.2f"
                    "}",
                    custId,
                    firstName,
                    lastName,
                    phone,
                    email,
                    balance
                    );
                    pthread_mutex_unlock(&bucket_locks[lock_index]);
                    return nextPtr;
                }
                nextPtr = nextPtr->next;
            }
       }
    }   
    pthread_mutex_unlock(&bucket_locks[lock_index]);
    return NULL;
}

/**
 * Below methods that will be called through FFI.
 */

void initHashTable() {
    init_hash_table();
}

void onboardCustomer(int custId, char firstName[MAX_NAME_CHARS], char lastName[MAX_NAME_CHARS],
char phone[MAX_PHONE_CHARS], char email[MAX_EMAIL_CHARS], double balance) {
    update_hash_table(custId, firstName, lastName, phone, email, balance);
}

/**
 * Why is the returnJsonBasicInfo being passed from Java ? 
 * Observe the methods that update entries into your hash_table. 
 * 
 * To ensure data is not corrupted during reading/writing by concurrent running Java threads, 
 * we are using pthread_mutex_init lock and unlock mechanism. This ensures that writing and reading
 * for same custId is mutually exclusive.
 * 
 * During lookup (read operation), we lock and unlock after finding the value for the looked up key (custId).
 * After unlock by read, a write operation may free the structure for that custId and refresh (again malloc) the  
 * hadshmap entry for that custId. This means, the returned value to Java may get freed by the time Java is reading it.
 * This will end up in Java getting corrupted value for reads. 
 * 
 * Now, if we decide to not free the memory during key overide during write, then the whole implementation logic in write
 * has to be changed.
 * Also, even if we ignore write operation for a moment, the value read from hash_table can not be cleared till Java has finished
 * reading it. Java will be responsible in this case to clear the memory (the memory occupied by the returned C char*). Means, after 
 * Java reads it, you need to expose another FFI method so thast Java can invoke it with input as the returned C char*;
 * and in C code this can be freed. This is not a neat approach. 
 * 
 * So instead, we are asking Java to pass a char* that C will fill and return. This ensures that memory for
 * returned json string is owned by Java (JVM) and C side does not have to deal with memory mgmnt for this variable. 
 * That is exactly what returnJsonBasicInfo is meant to do.
 * 
 * 
 */
void lookUpBasicCustomerInfo(int custId, char* returnJsonBasicInfo) {
    hash_table_look_up(custId, returnJsonBasicInfo);//return customer_info* is unused in this case.
}

void updateBasicCustomerInfo(int custId, char* phone, char* email)  {
    char jsonStr[CUSTOMER_INFO_JSON_STR_SIZE];//on stack, unused in this case, but have to pass it as per definition of hash_table_look_up method.
    customer_info* existingCustomerInfo = hash_table_look_up(custId, jsonStr);
    update_hash_table(custId, existingCustomerInfo->firstName, existingCustomerInfo->lastName, phone, email, existingCustomerInfo->balance);
}

//end FFI methods
int main(int argc, char const *argv[]){
    initHashTable();

    char firstName[MAX_NAME_CHARS] = "Binita";
    char lastName[MAX_NAME_CHARS] = "BHarati";
    char phone[MAX_PHONE_CHARS] = "984516786";
    char email[MAX_EMAIL_CHARS] = "binita@gmail.com";

    onboardCustomer(1, firstName, lastName, phone, email, 100.45);
    char jsonStr[CUSTOMER_INFO_JSON_STR_SIZE];
    lookUpBasicCustomerInfo(1, jsonStr);
    printf("existing customer info = %s\n",jsonStr);

    char phone1[MAX_PHONE_CHARS] = "778877889933";
    char email1[MAX_EMAIL_CHARS] = "bharati@gmail.com";
    updateBasicCustomerInfo(1, phone1, email1); 
    char jsonStr1[CUSTOMER_INFO_JSON_STR_SIZE];
    lookUpBasicCustomerInfo(1, jsonStr1);
    printf("updated customer info = %s\n",jsonStr1);

    return 0;

}

