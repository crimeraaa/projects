// File-scoped namespaces: everything after this point is part of this namespace!
namespace Fibonacci;

// Note that everything must be inside a class.
// Although like in C++ we can declare functions as static to indicate
// they aren't tied to any class instance (and thus don't take a this pointer).
class Fibonacci {
    // In C#, `Main` is a keyword: our entry point, and there can only be 1.
    static int Main(string[] args) {
        int rounds = GetSequence("Enter rounds: ");
        return WriteSequence(rounds);
    }
    /** Good lord this is ugly! I don't wanna write faux-HTML everywhere...
    <summary>
        Prints `rounds` number of the Fibonacci sequence.
    </summary>
    <param name="rounds">
        How many Fibonacci numbers to print.
    </param>
    <returns>
        0 if successful, 1 if a warning was printed.
    </returns>
     */
    public static int WriteSequence(int rounds) {
        UInt128 x = 0;
        UInt128 y = 1;
        UInt128 z = x + y;
        for (int i = 0; i < rounds; i++) {
            // Evaluate `z` separately, else order of assignment goes bad.
            Console.WriteLine("@" + i + ": " + x);
            x = y; 
            y = z;
            try {
                z = checked(x + y); // Keyword, can throw arith. overflow errors.
            } catch (OverflowException error) {
                Console.WriteLine(error.Message);
                return 1;
            }
        }
        return 0;
    }

    /**
    <summary>
        Prompts user for how many numbers in the Fibonacci sequence we should write.
    </summary>
    <remarks>
        Catches System.FormatException and System.OverflowException and tries again.
    </remarks>
    <param name="prompt">
        Prompt string to indicate something should be input.
    </param>
      */
    public static int GetSequence(string prompt) {
        int rounds = 0;
        while (rounds <= 0) {
            try {
                Console.Write(prompt);
                rounds = Convert.ToInt32(Console.ReadLine());
            } catch (FormatException error) {
                Console.WriteLine(error.Message);
            } catch (OverflowException error) {
                Console.WriteLine(error.Message);
            }
        }
        return rounds;
    }
}
