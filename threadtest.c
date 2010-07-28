/* Test if gcc supports thread */
int __thread x;
int main(void) { return x; }
